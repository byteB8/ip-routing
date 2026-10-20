#ifndef BSPL_H
#define BSPL_H

#include "hash_table.h"
#include "ipv4.h"

#include <cstdint>
#include <vector>

// Longest-prefix match by binary search on prefix lengths (Waldvogel et al., SIGCOMM 1997).
//
// There is one hash table per distinct prefix length. A lookup probes the table of the middle length:
// on a hit, a longer match may exist, so the search moves to the longer lengths; on a miss it moves to
// the shorter ones. For a long prefix to be reachable, every shorter length on its search path holds a
// marker, an entry that sends the search longer. A marker can also send the search towards prefixes
// that do not match the address, so every entry stores its best match: the longest real prefix that
// covers it. A lookup returns the best match of the last entry it hit, in at most ceil(log2(L + 1))
// probes for L distinct lengths.
//
// The tables are built once from the full prefix list: adding a prefix later can change the markers
// and best matches of many entries. Table is the per-length hash table, FlatTable or ChainedTable.
template <class Table>
class BasicBSPL {
 public:
  // lookup() returns an index into prefixes. If a prefix appears more than once, the last copy wins.
  explicit BasicBSPL(const std::vector<Prefix> &prefixes);

  int32_t lookup(uint32_t addr) const;  // route of the longest matching prefix, or -1
  int probes(uint32_t addr) const;      // hash probes a lookup of addr makes, for measurement
  size_t lengthCount() const { return lengths_.size(); }
  size_t entryCount() const;  // real prefixes and markers
  size_t markerCount() const { return markers_; }
  size_t bytes() const;  // memory of the hash tables

 private:
  // Key of an address in the table for prefix length len: its first len bits.
  static uint32_t key(uint32_t addr, int len) { return len == 0 ? 0u : addr >> (32 - len); }

  std::vector<int> lengths_;   // distinct prefix lengths, ascending
  std::vector<Table> tables_;  // tables_[i] holds the keys of length lengths_[i]
  size_t markers_ = 0;         // entries that are markers only
};

using BSPLTable = BasicBSPL<FlatTable>;
using ChainedBSPLTable = BasicBSPL<ChainedTable>;  // the same search on std::unordered_map, for comparison

#endif
