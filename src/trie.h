#ifndef TRIE_H
#define TRIE_H

#include "ipv4.h"

#include <cstdint>
#include <vector>

// Longest-prefix match with a binary trie: one address bit per level, so a lookup reads at most
// 33 nodes (the root and one per bit). Nodes live in one vector and refer to their children by
// index, so the whole trie is a single allocation and each link takes 4 bytes.
class IPTrie {
 public:
  // lookup() returns an index into prefixes. If a prefix appears more than once, the last copy wins.
  explicit IPTrie(const std::vector<Prefix> &prefixes = {});

  void insert(const Prefix &p, int32_t route);
  int32_t lookup(uint32_t addr) const;    // route of the longest matching prefix, or -1
  int nodesVisited(uint32_t addr) const;  // nodes a lookup of addr reads, for measurement
  size_t nodeCount() const { return nodes_.size(); }
  size_t bytes() const { return nodes_.size() * sizeof(Node); }

 private:
  struct Node {
    int32_t child[2] = {-1, -1};
    int32_t route = -1;
  };
  std::vector<Node> nodes_;
};

#endif
