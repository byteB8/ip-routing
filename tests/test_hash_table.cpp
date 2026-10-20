#include "check.h"
#include "hash_table.h"

#include <random>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace {

int32_t valueFor(uint32_t key) { return static_cast<int32_t>(key & 0x7FFFFFFF); }

template <class Table>
void roundTrip() {
  Table table;
  std::mt19937 rng(7);
  std::vector<uint32_t> keys = {0u, 1u, 0xFFFFFFFFu};
  for (int i = 0; i < 100000; ++i)
    keys.push_back(static_cast<uint32_t>(rng()));
  for (uint32_t k = 5000; k < 6000; ++k)  // consecutive keys, as prefixes of one length often are
    keys.push_back(k);
  std::unordered_set<uint32_t> distinct(keys.begin(), keys.end());

  size_t badStart = 0;
  for (uint32_t k : keys) {
    BSPLEntry &e = table[k];
    badStart += e.route != -1 && e.route != valueFor(k);  // a new key starts as a marker
    e.route = valueFor(k);
  }
  CHECK_EQ(badStart, 0u);
  CHECK_EQ(table.size(), distinct.size());

  size_t wrong = 0;
  for (uint32_t k : keys) {
    const BSPLEntry *e = table.find(k);
    wrong += !e || e->route != valueFor(k) || e->bestMatch != -1;
  }
  CHECK_EQ(wrong, 0u);

  size_t falseHits = 0;
  for (int i = 0; i < 100000; ++i) {
    uint32_t k = static_cast<uint32_t>(rng());
    falseHits += !distinct.count(k) && table.find(k);
  }
  CHECK_EQ(falseHits, 0u);

  size_t visited = 0;
  table.forEach([&](uint32_t k, BSPLEntry &e) { visited += distinct.count(k) && e.route == valueFor(k); });
  CHECK_EQ(visited, distinct.size());

  if constexpr (std::is_same_v<Table, FlatTable>)
    CHECK(table.bytes() >= 2 * 12 * table.size());  // at most half full, 12-byte slots
}

}  // namespace

int main() {
  roundTrip<FlatTable>();
  roundTrip<ChainedTable>();
  return report("test_hash_table");
}
