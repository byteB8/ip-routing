#include "bspl.h"

#include <array>

template <class Table>
BasicBSPL<Table>::BasicBSPL(const std::vector<Prefix> &prefixes) {
  std::array<bool, 33> present{};
  for (const Prefix &p : prefixes)
    present[p.len] = true;
  std::array<int, 33> index{};
  for (int len = 0; len <= 32; ++len)
    if (present[len]) {
      index[len] = static_cast<int>(lengths_.size());
      lengths_.push_back(len);
    }
  tables_.resize(lengths_.size());

  // Real prefixes. Assigning in order lets a repeated prefix keep its last route.
  for (size_t i = 0; i < prefixes.size(); ++i) {
    const Prefix &p = prefixes[i];
    tables_[index[p.len]][key(p.addr, p.len)].route = static_cast<int32_t>(i);
  }

  // Markers: walk each prefix's search path and leave an entry at every shorter length it probes,
  // so that a lookup of an address under the prefix is sent on towards it.
  for (const Prefix &p : prefixes) {
    int lo = 0, hi = static_cast<int>(lengths_.size()) - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      if (lengths_[mid] == p.len)
        break;
      if (lengths_[mid] < p.len) {
        tables_[mid][key(p.addr, lengths_[mid])];  // adds a marker unless the key is already there
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
  }

  // Best matches: for every entry, the longest real prefix at its own length or a shorter one that
  // covers it.
  for (size_t i = 0; i < tables_.size(); ++i)
    tables_[i].forEach([&](uint32_t k, BSPLEntry &entry) {
      markers_ += entry.route < 0;
      for (int j = static_cast<int>(i); j >= 0; --j) {
        uint32_t shorter = lengths_[j] == 0 ? 0u : k >> (lengths_[i] - lengths_[j]);
        const BSPLEntry *e = tables_[j].find(shorter);
        if (e && e->route >= 0) {
          entry.bestMatch = e->route;
          break;
        }
      }
    });
}

template <class Table>
int32_t BasicBSPL<Table>::lookup(uint32_t addr) const {
  int32_t best = -1;
  int lo = 0, hi = static_cast<int>(lengths_.size()) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (const BSPLEntry *e = tables_[mid].find(key(addr, lengths_[mid]))) {
      best = e->bestMatch;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return best;
}

template <class Table>
int BasicBSPL<Table>::probes(uint32_t addr) const {
  int count = 0;
  int lo = 0, hi = static_cast<int>(lengths_.size()) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    ++count;
    if (tables_[mid].find(key(addr, lengths_[mid])))
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return count;
}

template <class Table>
size_t BasicBSPL<Table>::entryCount() const {
  size_t n = 0;
  for (const Table &table : tables_)
    n += table.size();
  return n;
}

template <class Table>
size_t BasicBSPL<Table>::bytes() const {
  size_t n = 0;
  for (const Table &table : tables_)
    n += table.bytes();
  return n;
}

template class BasicBSPL<FlatTable>;
template class BasicBSPL<ChainedTable>;
