#include "bspl.h"
#include "check.h"
#include "ipv4.h"
#include "trie.h"

#include <initializer_list>
#include <iostream>
#include <random>
#include <vector>

namespace {

std::vector<Prefix> prefixes(std::initializer_list<const char *> texts) {
  std::vector<Prefix> out;
  for (const char *text : texts)
    out.push_back(*parsePrefix(text));
  return out;
}

template <class Table>
int32_t route(const Table &table, const char *addr) {
  return table.lookup(*parseAddress(addr));
}

template <class Table>
void fixedCases() {
  // the sample table, data/sample.csv
  Table sample(prefixes({"192.168.20.16/28", "192.168.0.0/16", "192.168.1.0/24", "10.10.0.0/16"}));
  CHECK_EQ(route(sample, "192.168.20.20"), 0);  // inside both the /28 and the /16: the /28 wins
  CHECK_EQ(route(sample, "192.168.20.31"), 0);
  CHECK_EQ(route(sample, "192.168.20.191"), 1);  // inside the /16 only
  CHECK_EQ(route(sample, "192.168.20.15"), 1);
  CHECK_EQ(route(sample, "192.168.1.5"), 2);
  CHECK_EQ(route(sample, "10.10.3.4"), 3);
  CHECK_EQ(route(sample, "8.8.8.8"), -1);

  Table empty({});
  CHECK_EQ(route(empty, "1.2.3.4"), -1);

  // a default route matches every address; a /32 matches one
  Table edges(prefixes({"0.0.0.0/0", "10.1.2.3/32"}));
  CHECK_EQ(route(edges, "10.1.2.3"), 1);
  CHECK_EQ(route(edges, "10.1.2.4"), 0);
  CHECK_EQ(route(edges, "255.255.255.255"), 0);

  // nested prefixes: each address takes the longest one that covers it
  Table chain(prefixes({"10.0.0.0/8", "10.20.0.0/16", "10.20.30.0/24", "10.20.30.40/32", "10.20.30.0/25"}));
  CHECK_EQ(route(chain, "10.99.0.1"), 0);
  CHECK_EQ(route(chain, "10.20.99.1"), 1);
  CHECK_EQ(route(chain, "10.20.30.200"), 2);
  CHECK_EQ(route(chain, "10.20.30.40"), 3);
  CHECK_EQ(route(chain, "10.20.30.41"), 4);

  // a repeated prefix: the last copy wins
  Table repeated(prefixes({"172.16.0.0/12", "172.16.0.0/12"}));
  CHECK_EQ(route(repeated, "172.20.1.1"), 1);
}

template <class Table>
void bsplStructure() {
  // Sample table: lengths 16, 24, 28. The /28's search path probes /24 first, which needs a marker
  // for 192.168.20 there; its best match is the /16.
  Table sample(prefixes({"192.168.20.16/28", "192.168.0.0/16", "192.168.1.0/24", "10.10.0.0/16"}));
  CHECK_EQ(sample.lengthCount(), 3u);
  CHECK_EQ(sample.entryCount(), 5u);
  CHECK_EQ(sample.markerCount(), 1u);
  CHECK_EQ(sample.probes(*parseAddress("192.168.20.20")), 2);   // /24 marker, then the /28
  CHECK_EQ(sample.probes(*parseAddress("192.168.20.191")), 2);  // /24 marker, a miss at /28
  CHECK_EQ(sample.probes(*parseAddress("8.8.8.8")), 2);         // misses at /24 and /16
}

// Reference answer: scan every prefix and keep the longest that contains the address
// (for a repeated prefix, the last copy).
int32_t linearLookup(const std::vector<Prefix> &table, uint32_t addr) {
  int32_t best = -1;
  for (size_t i = 0; i < table.size(); ++i)
    if (contains(table[i], addr) && (best < 0 || table[i].len >= table[best].len))
      best = static_cast<int32_t>(i);
  return best;
}

uint32_t random32(std::mt19937 &rng) { return static_cast<uint32_t>(rng()); }

// A random table where most prefixes are derived from one already present, either nested under it
// or covering it, so deep chains of nested prefixes, siblings and repeats are common. A quarter of
// the tables also get a default route.
std::vector<Prefix> randomTable(std::mt19937 &rng, size_t n) {
  std::uniform_int_distribution<int> anyLen(1, 32);
  std::vector<Prefix> table;
  while (table.size() < n) {
    int len = anyLen(rng);
    if (table.empty() || rng() % 4 == 0) {
      table.push_back({random32(rng) & mask(len), len});
    } else {
      const Prefix base = table[rng() % table.size()];
      uint32_t addr = len > base.len ? base.addr | (random32(rng) & ~mask(base.len)) : base.addr;
      table.push_back({addr & mask(len), len});
    }
  }
  if (rng() % 4 == 0)
    table[rng() % table.size()] = Prefix{0, 0};
  return table;
}

// Addresses to look up: uniform ones, ones inside the table's prefixes, and the first address, last
// address and the address after the last of a prefix, where off-by-one mistakes show.
std::vector<uint32_t> randomQueries(std::mt19937 &rng, const std::vector<Prefix> &table, size_t n) {
  std::vector<uint32_t> out;
  while (out.size() < n) {
    const Prefix &p = table[rng() % table.size()];
    switch (rng() % 4) {
      case 0: out.push_back(random32(rng)); break;
      case 1: out.push_back(p.addr | (random32(rng) & ~mask(p.len))); break;
      case 2: out.push_back(p.addr); break;
      default:
        out.push_back(p.addr | ~mask(p.len));
        out.push_back((p.addr | ~mask(p.len)) + 1);  // wraps to 0.0.0.0 after 255.255.255.255
    }
  }
  return out;
}

// Every table must agree with the linear scan on every address, and a BSPL lookup must take at most
// ceil(log2(L + 1)) probes for L distinct prefix lengths.
void differential() {
  std::mt19937 rng(20261020);
  size_t tables = 0, addresses = 0, mismatches = 0, overProbes = 0;
  for (int t = 0; t < 1000; ++t) {
    size_t n = 1 + rng() % (t < 500 ? 16 : 1000);  // small tables reach corner cases; large ones nest deeply
    std::vector<Prefix> table = randomTable(rng, n);
    IPTrie trie(table);
    BSPLTable bspl(table);
    ChainedBSPLTable chained(table);
    int probeLimit = 0;
    while ((1u << probeLimit) < bspl.lengthCount() + 1)
      ++probeLimit;
    for (uint32_t addr : randomQueries(rng, table, 1000)) {
      int32_t want = linearLookup(table, addr);
      mismatches += (trie.lookup(addr) != want) + (bspl.lookup(addr) != want) + (chained.lookup(addr) != want);
      overProbes += bspl.probes(addr) > probeLimit;
      ++addresses;
    }
    ++tables;
  }
  CHECK_EQ(mismatches, 0u);
  CHECK_EQ(overProbes, 0u);
  std::cout << "differential: " << addresses << " addresses on " << tables
            << " random tables, each looked up in all three tables: " << mismatches << " mismatches\n";
}

}  // namespace

int main() {
  fixedCases<IPTrie>();
  fixedCases<BSPLTable>();
  fixedCases<ChainedBSPLTable>();
  bsplStructure<BSPLTable>();
  bsplStructure<ChainedBSPLTable>();
  differential();
  return report("test_lpm");
}
