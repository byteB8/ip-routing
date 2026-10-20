#include "bspl.h"
#include "check.h"
#include "ipv4.h"
#include "trie.h"

#include <initializer_list>
#include <vector>

namespace {

std::vector<Prefix> prefixes(std::initializer_list<const char *> texts) {
  std::vector<Prefix> out;
  for (const char *text : texts)
    out.push_back(*parsePrefix(text));
  return out;
}

template <class Table>
int32_t route(const Table &table, const char *addr) {
  return table.lookup(*parseAddress(addr));
}

template <class Table>
void fixedCases() {
  // the sample table, data/sample.csv
  Table sample(prefixes({"192.168.20.16/28", "192.168.0.0/16", "192.168.1.0/24", "10.10.0.0/16"}));
  CHECK_EQ(route(sample, "192.168.20.20"), 0);  // inside both the /28 and the /16: the /28 wins
  CHECK_EQ(route(sample, "192.168.20.31"), 0);
  CHECK_EQ(route(sample, "192.168.20.191"), 1);  // inside the /16 only
  CHECK_EQ(route(sample, "192.168.20.15"), 1);
  CHECK_EQ(route(sample, "192.168.1.5"), 2);
  CHECK_EQ(route(sample, "10.10.3.4"), 3);
  CHECK_EQ(route(sample, "8.8.8.8"), -1);

  Table empty({});
  CHECK_EQ(route(empty, "1.2.3.4"), -1);

  // a default route matches every address; a /32 matches one
  Table edges(prefixes({"0.0.0.0/0", "10.1.2.3/32"}));
  CHECK_EQ(route(edges, "10.1.2.3"), 1);
  CHECK_EQ(route(edges, "10.1.2.4"), 0);
  CHECK_EQ(route(edges, "255.255.255.255"), 0);

  // nested prefixes: each address takes the longest one that covers it
  Table chain(prefixes({"10.0.0.0/8", "10.20.0.0/16", "10.20.30.0/24", "10.20.30.40/32", "10.20.30.0/25"}));
  CHECK_EQ(route(chain, "10.99.0.1"), 0);
  CHECK_EQ(route(chain, "10.20.99.1"), 1);
  CHECK_EQ(route(chain, "10.20.30.200"), 2);
  CHECK_EQ(route(chain, "10.20.30.40"), 3);
  CHECK_EQ(route(chain, "10.20.30.41"), 4);

  // a repeated prefix: the last copy wins
  Table repeated(prefixes({"172.16.0.0/12", "172.16.0.0/12"}));
  CHECK_EQ(route(repeated, "172.20.1.1"), 1);
}

void bsplStructure() {
  // Sample table: lengths 16, 24, 28. The /28's search path probes /24 first, which needs a marker
  // for 192.168.20 there; its best match is the /16.
  BSPLTable sample(prefixes({"192.168.20.16/28", "192.168.0.0/16", "192.168.1.0/24", "10.10.0.0/16"}));
  CHECK_EQ(sample.lengthCount(), 3u);
  CHECK_EQ(sample.entryCount(), 5u);
  CHECK_EQ(sample.markerCount(), 1u);
  CHECK_EQ(sample.probes(*parseAddress("192.168.20.20")), 2);   // /24 marker, then the /28
  CHECK_EQ(sample.probes(*parseAddress("192.168.20.191")), 2);  // /24 marker, a miss at /28
  CHECK_EQ(sample.probes(*parseAddress("8.8.8.8")), 2);         // misses at /24 and /16
}

}  // namespace

int main() {
  fixedCases<IPTrie>();
  fixedCases<BSPLTable>();
  bsplStructure();
  return report("test_lpm");
}
