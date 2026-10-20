// Longest-prefix-match lookups against a CSV routing table.
//
//   iproute <table.csv> [address ...]
//
// Looks up each address given (or, if none, each line of standard input) and prints the route chosen.
#include "ipv4.h"
#include "routing_table.h"
#include "trie.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: iproute <table.csv> [address ...]\n";
    return 2;
  }
  std::vector<Route> routes;
  try {
    routes = loadRoutes(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << "iproute: " << e.what() << "\n";
    return 1;
  }
  IPTrie table(prefixesOf(routes));

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

  if (argc > 2) {
    for (int i = 2; i < argc; ++i)
      lookup(argv[i]);
  } else {
    for (std::string line; std::getline(std::cin, line);)
      if (!line.empty())
        lookup(line);
  }
  return status;
}
