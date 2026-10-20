#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <cstdint>
#include <unordered_map>
#include <vector>

// An entry of a BSPL hash table: a real prefix, a marker, or both.
struct BSPLEntry {
  int32_t route = -1;      // route of the real prefix with this key; -1 for a marker
  int32_t bestMatch = -1;  // route of the longest real prefix covering this key; -1 if none
};

// Two hash tables from 32-bit keys to entries, with the same small interface for BSPL:
// operator[] inserts a default entry if the key is absent, and find returns nullptr if it is absent.

// Separate chaining with std::unordered_map: each entry lives in its own heap node, so a probe
// reads the bucket array and then follows a pointer to the node.
class ChainedTable {
 public:
  BSPLEntry &operator[](uint32_t key) { return map_[key]; }
  const BSPLEntry *find(uint32_t key) const {
    auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }
  template <class F>
  void forEach(F f) {
    for (auto &[key, entry] : map_)
      f(key, entry);
  }
  size_t size() const { return map_.size(); }
  size_t bytes() const;  // estimated: nodes and bucket array, without allocator overhead

 private:
  std::unordered_map<uint32_t, BSPLEntry> map_;
};

// Open addressing with linear probing: keys and entries sit in one array of 12-byte slots, so a probe
// usually reads a single cache line. The capacity is a power of two and the table is kept at most
// half full, which keeps probe sequences short. References returned by operator[] are invalidated
// when the table grows.
class FlatTable {
 public:
  BSPLEntry &operator[](uint32_t key);
  const BSPLEntry *find(uint32_t key) const {
    for (size_t i = home(key);; i = (i + 1) & (slots_.size() - 1)) {
      const Slot &s = slots_[i];
      if (s.empty())
        return nullptr;
      if (s.key == key)
        return &s.entry;
    }
  }
  template <class F>
  void forEach(F f) {
    for (Slot &s : slots_)
      if (!s.empty())
        f(s.key, s.entry);
  }
  size_t size() const { return size_; }
  size_t bytes() const { return slots_.size() * sizeof(Slot); }

 private:
  static constexpr int32_t kEmpty = -2;  // route of an unused slot: real routes are >= 0, markers -1

  struct Slot {
    uint32_t key = 0;
    BSPLEntry entry{kEmpty, -1};
    bool empty() const { return entry.route == kEmpty; }
  };

  // Fibonacci hashing: multiply by 2^64 divided by the golden ratio and keep the top bits, which
  // spreads prefix keys (often consecutive numbers) over the whole table.
  size_t home(uint32_t key) const { return (key * 0x9E3779B97F4A7C15ull) >> shift_; }
  void grow();

  std::vector<Slot> slots_ = std::vector<Slot>(16);
  size_t size_ = 0;
  int shift_ = 60;  // 64 - log2(capacity)
};

#endif
