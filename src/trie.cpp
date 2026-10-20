#include "trie.h"

IPTrie::IPTrie(const std::vector<Prefix> &prefixes) : nodes_(1) {
  for (size_t i = 0; i < prefixes.size(); ++i)
    insert(prefixes[i], static_cast<int32_t>(i));
}

void IPTrie::insert(const Prefix &p, int32_t route) {
  int32_t node = 0;
  for (int i = 0; i < p.len; ++i) {
    int bit = (p.addr >> (31 - i)) & 1;
    if (nodes_[node].child[bit] < 0) {
      nodes_[node].child[bit] = static_cast<int32_t>(nodes_.size());
      nodes_.emplace_back();  // may reallocate, so nodes are reached by index, never by reference
    }
    node = nodes_[node].child[bit];
  }
  nodes_[node].route = route;
}

int32_t IPTrie::lookup(uint32_t addr) const {
  int32_t node = 0;
  int32_t best = nodes_[0].route;  // a /0 route matches every address
  for (int i = 0; i < 32; ++i) {
    node = nodes_[node].child[(addr >> (31 - i)) & 1];
    if (node < 0)
      break;
    if (nodes_[node].route >= 0)
      best = nodes_[node].route;
  }
  return best;
}

int IPTrie::nodesVisited(uint32_t addr) const {
  int32_t node = 0;
  int visited = 1;
  for (int i = 0; i < 32; ++i) {
    node = nodes_[node].child[(addr >> (31 - i)) & 1];
    if (node < 0)
      break;
    ++visited;
  }
  return visited;
}
