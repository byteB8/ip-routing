#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

#include "ipv4.h"

#include <string>
#include <vector>

struct Route {
  Prefix prefix;
  std::string nextHop;
};

// Reads a routing table from CSV: a header line, then one "prefix,next_hop" row per route, such as
// "192.168.1.0/24,Router3". Blank lines are skipped. A malformed row throws std::runtime_error
// naming the file and line.
std::vector<Route> loadRoutes(const std::string &path);

// The routes' prefixes, in order: the input the lookup structures take.
std::vector<Prefix> prefixesOf(const std::vector<Route> &routes);

#endif
