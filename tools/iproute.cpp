// Longest-prefix-match lookups against a CSV routing table.
//
//   iproute [--bspl] <table.csv> [address ...]
//
// Looks up each address given (or, if none, each line of standard input) and prints the route chosen.
// The binary trie answers by default; --bspl uses binary search on prefix lengths instead.
#include "bspl.h"
#include "ipv4.h"
#include "routing_table.h"
#include "trie.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

template <class Table>
int run(const Table &table, const std::vector<Route> &routes, char **first, char **last) {
  int status = 0;
  auto lookup = [&](std::string text) {
    if (!text.empty() && text.back() == '\r')
      text.pop_back();
    auto addr = parseAddress(text);
    if (!addr) {
      std::cerr << "iproute: not an IPv4 address: \"" << text << "\"\n";
      status = 1;
      return;
    }
    int32_t r = table.lookup(*addr);
    if (r < 0)
      std::cout << text << " -> no route\n";
    else
      std::cout << text << " -> " << routes[r].nextHop << " via " << formatPrefix(routes[r].prefix) << "\n";
  };

  if (first != last) {
    for (char **arg = first; arg != last; ++arg)
      lookup(*arg);
  } else {
    for (std::string line; std::getline(std::cin, line);)
      if (!line.empty())
        lookup(line);
  }
  return status;
}

}  // namespace

int main(int argc, char **argv) {
  bool bspl = argc > 1 && std::string(argv[1]) == "--bspl";
  int tableArg = bspl ? 2 : 1;
  if (argc <= tableArg) {
    std::cerr << "usage: iproute [--bspl] <table.csv> [address ...]\n";
    return 2;
  }
  std::vector<Route> routes;
  try {
    routes = loadRoutes(argv[tableArg]);
  } catch (const std::exception &e) {
    std::cerr << "iproute: " << e.what() << "\n";
    return 1;
  }
  std::vector<Prefix> prefixes = prefixesOf(routes);
  if (bspl)
    return run(BSPLTable(prefixes), routes, argv + tableArg + 1, argv + argc);
  return run(IPTrie(prefixes), routes, argv + tableArg + 1, argv + argc);
}
