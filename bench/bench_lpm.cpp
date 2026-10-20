// Lookup benchmark for the binary trie and BSPL (open addressing and std::unordered_map) on a routing
// table and on random subsets of it.
//
//   bench_lpm <table.csv> <out_dir>
//
// Writes lookup.csv, memory.csv, steps.csv and environment.txt to out_dir and prints a summary.
// A timing is the median of 11 passes over 1M addresses, after one warm-up pass. The passes of the
// three structures are interleaved, so slow spells of the machine (clock changes, other processes)
// hit all three alike. In an "independent" pass the CPU can overlap consecutive lookups; in a
// "dependent" pass each lookup's address depends on the previous result, so the pass measures the
// latency of one lookup.
#include "bspl.h"
#include "ipv4.h"
#include "routing_table.h"
#include "trie.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr size_t kLookups = 1'000'000;
constexpr int kRounds = 11;
using Clock = std::chrono::steady_clock;

volatile uint32_t zeroAtRunTime = 0;  // always 0, but the compiler cannot assume it
uint64_t sink = 0;                    // keeps lookup results alive; printed at the end

struct Timing {
  double median, min, max;
};

// Runs each pass once to warm up, then kRounds times, one pass of each per round in rotating order.
std::vector<Timing> measureInterleaved(const std::vector<std::function<double()>> &passes) {
  const size_t n = passes.size();
  std::vector<std::vector<double>> ns(n);
  for (const auto &pass : passes)
    pass();
  for (int round = 0; round < kRounds; ++round)
    for (size_t i = 0; i < n; ++i) {
      size_t which = (round + i) % n;
      ns[which].push_back(passes[which]());
    }
  std::vector<Timing> timings;
  for (auto &v : ns) {
    std::sort(v.begin(), v.end());
    timings.push_back({v[v.size() / 2], v.front(), v.back()});
  }
  return timings;
}

double elapsedNs(Clock::time_point start) {
  return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

template <class Table>
double independentPass(const Table &table, const std::vector<uint32_t> &addrs) {
  uint64_t sum = 0;
  auto start = Clock::now();
  for (uint32_t a : addrs)
    sum += static_cast<uint32_t>(table.lookup(a));
  double ns = elapsedNs(start);
  sink += sum;
  return ns / addrs.size();
}

// Each address is XORed with (previous result & zero). That is always 0, but the CPU only learns it
// once the previous lookup has finished, so lookups cannot overlap.
template <class Table>
double dependentPass(const Table &table, const std::vector<uint32_t> &addrs) {
  const uint32_t zero = zeroAtRunTime;
  uint32_t carry = 0;
  uint64_t sum = 0;
  auto start = Clock::now();
  for (uint32_t a : addrs) {
    int32_t r = table.lookup(a ^ carry);
    carry = static_cast<uint32_t>(r) & zero;
    sum += static_cast<uint32_t>(r);
  }
  double ns = elapsedNs(start);
  sink += sum;
  return ns / addrs.size();
}

// Addresses inside the table's prefixes: a prefix chosen uniformly, then random host bits.
std::vector<uint32_t> routedAddresses(const std::vector<Prefix> &table, std::mt19937 &rng) {
  std::vector<uint32_t> addrs(kLookups);
  for (uint32_t &a : addrs) {
    const Prefix &p = table[rng() % table.size()];
    a = p.addr | (static_cast<uint32_t>(rng()) & ~mask(p.len));
  }
  return addrs;
}

std::vector<uint32_t> uniformAddresses(std::mt19937 &rng) {
  std::vector<uint32_t> addrs(kLookups);
  for (uint32_t &a : addrs)
    a = static_cast<uint32_t>(rng());
  return addrs;
}

// Builds the table three times and reports the fastest build.
template <class Table>
Table build(const std::vector<Prefix> &prefixes, double &bestMs) {
  bestMs = 1e30;
  for (int i = 0; i < 2; ++i) {
    auto start = Clock::now();
    Table discarded(prefixes);
    bestMs = std::min(bestMs, elapsedNs(start) / 1e6);
    sink += static_cast<uint32_t>(discarded.lookup(0));
  }
  auto start = Clock::now();
  Table table(prefixes);
  bestMs = std::min(bestMs, elapsedNs(start) / 1e6);
  return table;
}

std::string cpuModel() {
  std::ifstream info("/proc/cpuinfo");
  for (std::string line; std::getline(info, line);)
    if (line.rfind("model name", 0) == 0)
      return line.substr(line.find(':') + 2);
  return "unknown";
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: bench_lpm <table.csv> <out_dir>\n";
    return 2;
  }
  const std::string out = argv[2];
  std::vector<Prefix> full = prefixesOf(loadRoutes(argv[1]));

  std::ofstream lookupCsv(out + "/lookup.csv"), memoryCsv(out + "/memory.csv"), stepsCsv(out + "/steps.csv");
  if (!lookupCsv || !memoryCsv || !stepsCsv) {
    std::cerr << "bench_lpm: cannot write to " << out << "\n";
    return 1;
  }
  lookupCsv << "routes,structure,addresses,mode,ns_median,ns_min,ns_max\n";
  memoryCsv << "routes,structure,items,markers,bytes,bytes_per_route,build_ms\n";
  stepsCsv << "structure,addresses,steps,count\n";

  std::mt19937 rng(2026);
  std::vector<size_t> order(full.size());
  std::iota(order.begin(), order.end(), 0);
  std::shuffle(order.begin(), order.end(), rng);

  std::vector<size_t> sizes;
  for (size_t n : {1'000, 3'000, 10'000, 30'000, 100'000, 300'000})
    if (n < full.size())
      sizes.push_back(n);
  sizes.push_back(full.size());

  for (size_t n : sizes) {
    // a random subset keeps the full table's mix of prefix lengths
    std::vector<Prefix> table;
    if (n == full.size()) {
      table = full;
    } else {
      for (size_t i = 0; i < n; ++i)
        table.push_back(full[order[i]]);
    }
    double trieMs, flatMs, chainedMs;
    IPTrie trie = build<IPTrie>(table, trieMs);
    BSPLTable flat = build<BSPLTable>(table, flatMs);
    ChainedBSPLTable chained = build<ChainedBSPLTable>(table, chainedMs);
    auto memoryRow = [&](const char *structure, size_t items, size_t markers, size_t bytes, double ms) {
      memoryCsv << n << "," << structure << "," << items << "," << markers << "," << bytes << ","
                << double(bytes) / n << "," << ms << "\n";
    };
    memoryRow("trie", trie.nodeCount(), 0, trie.bytes(), trieMs);
    memoryRow("bspl_flat", flat.entryCount(), flat.markerCount(), flat.bytes(), flatMs);
    memoryRow("bspl_chained", chained.entryCount(), chained.markerCount(), chained.bytes(), chainedMs);

    std::vector<std::pair<std::string, std::vector<uint32_t>>> addressSets;
    addressSets.emplace_back("routed", routedAddresses(table, rng));
    if (n == full.size())
      addressSets.emplace_back("uniform", uniformAddresses(rng));

    for (const auto &[kind, addrs] : addressSets) {
      auto row = [&](const char *structure, const char *mode, Timing t) {
        lookupCsv << n << "," << structure << "," << kind << "," << mode << "," << t.median << "," << t.min << ","
                  << t.max << "\n";
        std::cout << n << " routes, " << structure << ", " << kind << ", " << mode << ": " << t.median
                  << " ns/lookup\n";
      };
      auto independent = measureInterleaved({[&] { return independentPass(trie, addrs); },
                                             [&] { return independentPass(flat, addrs); },
                                             [&] { return independentPass(chained, addrs); }});
      auto dependent = measureInterleaved({[&] { return dependentPass(trie, addrs); },
                                           [&] { return dependentPass(flat, addrs); },
                                           [&] { return dependentPass(chained, addrs); }});
      const char *names[] = {"trie", "bspl_flat", "bspl_chained"};
      for (int i = 0; i < 3; ++i)
        row(names[i], "independent", independent[i]);
      for (int i = 0; i < 3; ++i)
        row(names[i], "dependent", dependent[i]);

      if (n == full.size()) {
        std::map<int, size_t> trieSteps, bsplSteps;
        size_t matched = 0;
        for (uint32_t a : addrs) {
          ++trieSteps[trie.nodesVisited(a)];
          ++bsplSteps[flat.probes(a)];  // both BSPL tables probe the same lengths
          matched += trie.lookup(a) >= 0;
        }
        for (const auto &[steps, count] : trieSteps)
          stepsCsv << "trie," << kind << "," << steps << "," << count << "\n";
        for (const auto &[steps, count] : bsplSteps)
          stepsCsv << "bspl," << kind << "," << steps << "," << count << "\n";
        std::cout << "  " << kind << " addresses with a route: " << 100.0 * matched / addrs.size() << "%\n";
      }
    }
    std::cout << "  trie: " << trie.nodeCount() << " nodes, " << trie.bytes() / 1e6 << " MB, built in " << trieMs
              << " ms\n  bspl: " << flat.lengthCount() << " lengths, " << flat.entryCount() << " entries ("
              << flat.markerCount() << " markers); open addressing " << flat.bytes() / 1e6 << " MB, built in "
              << flatMs << " ms; std::unordered_map " << chained.bytes() / 1e6 << " MB, built in " << chainedMs
              << " ms\n";
  }

  std::ofstream env(out + "/environment.txt");
  env << "cpu: " << cpuModel() << "\ncompiler: g++ " << __VERSION__ << "\ntable: " << argv[1] << " ("
      << full.size() << " prefixes)\nlookups per pass: " << kLookups << ", passes: " << kRounds
      << " per structure after one warm-up, interleaved across structures, median reported\n";
  std::cout << "checksum " << sink << "\n";
}
