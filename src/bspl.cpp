#include "bspl.h"

#include <array>
#include <utility>

BSPLTable::BSPLTable(const std::vector<Prefix> &prefixes) {
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
        tables_[mid].try_emplace(key(p.addr, lengths_[mid]));
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
  }

  // Best matches: for every entry, the longest real prefix at its own length or a shorter one that
  // covers it.
  for (size_t i = 0; i < tables_.size(); ++i)
    for (auto &[k, entry] : tables_[i])
      for (int j = static_cast<int>(i); j >= 0; --j) {
        uint32_t shorter = lengths_[j] == 0 ? 0u : k >> (lengths_[i] - lengths_[j]);
        auto it = tables_[j].find(shorter);
        if (it != tables_[j].end() && it->second.route >= 0) {
          entry.bestMatch = it->second.route;
          break;
        }
      }
}

int32_t BSPLTable::lookup(uint32_t addr) const {
  int32_t best = -1;
  int lo = 0, hi = static_cast<int>(lengths_.size()) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    auto it = tables_[mid].find(key(addr, lengths_[mid]));
    if (it != tables_[mid].end()) {
      best = it->second.bestMatch;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return best;
}

int BSPLTable::probes(uint32_t addr) const {
  int count = 0;
  int lo = 0, hi = static_cast<int>(lengths_.size()) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    ++count;
    if (tables_[mid].count(key(addr, lengths_[mid])))
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return count;
}

size_t BSPLTable::entryCount() const {
  size_t n = 0;
  for (const auto &table : tables_)
    n += table.size();
  return n;
}

size_t BSPLTable::markerCount() const {
  size_t n = 0;
  for (const auto &table : tables_)
    for (const auto &kv : table)
      n += kv.second.route < 0;
  return n;
}

size_t BSPLTable::bytes() const {
  // A hash node holds a next pointer and the key/entry pair, padded to pointer alignment.
  constexpr size_t align = alignof(void *);
  constexpr size_t node = (sizeof(void *) + sizeof(std::pair<const uint32_t, Entry>) + align - 1) / align * align;
  size_t total = 0;
  for (const auto &table : tables_)
    total += table.bucket_count() * sizeof(void *) + table.size() * node;
  return total;
}
