#include "routing_table.h"

#include <fstream>
#include <stdexcept>

std::vector<Route> loadRoutes(const std::string &path) {
  std::ifstream file(path);
  if (!file)
    throw std::runtime_error("cannot open " + path);
  std::vector<Route> routes;
  std::string line;
  std::getline(file, line);  // header
  for (int lineNo = 2; std::getline(file, line); ++lineNo) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;
    size_t comma = line.find(',');
    auto prefix = parsePrefix(std::string_view(line).substr(0, comma));
    if (comma == std::string::npos || comma + 1 == line.size() || !prefix)
      throw std::runtime_error(path + ":" + std::to_string(lineNo) + ": expected prefix,next_hop, got \"" +
                               line + "\"");
    routes.push_back({*prefix, line.substr(comma + 1)});
  }
  return routes;
}

std::vector<Prefix> prefixesOf(const std::vector<Route> &routes) {
  std::vector<Prefix> prefixes;
  prefixes.reserve(routes.size());
  for (const Route &r : routes)
    prefixes.push_back(r.prefix);
  return prefixes;
}
