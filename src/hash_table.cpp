#include "hash_table.h"

#include <utility>

size_t ChainedTable::bytes() const {
  // a node holds a next pointer and the key/entry pair, padded to pointer alignment
  constexpr size_t align = alignof(void *);
  constexpr size_t node = (sizeof(void *) + sizeof(std::pair<const uint32_t, BSPLEntry>) + align - 1) / align * align;
  return map_.bucket_count() * sizeof(void *) + map_.size() * node;
}

BSPLEntry &FlatTable::operator[](uint32_t key) {
  size_t i = home(key);
  for (; !slots_[i].empty(); i = (i + 1) & (slots_.size() - 1))
    if (slots_[i].key == key)
      return slots_[i].entry;
  if (2 * (size_ + 1) > slots_.size()) {
    grow();
    return (*this)[key];
  }
  slots_[i].key = key;
  slots_[i].entry = BSPLEntry{};
  ++size_;
  return slots_[i].entry;
}

void FlatTable::grow() {
  std::vector<Slot> old(slots_.size() * 2);
  old.swap(slots_);
  --shift_;
  size_ = 0;
  for (const Slot &s : old)
    if (!s.empty())
      (*this)[s.key] = s.entry;
}
