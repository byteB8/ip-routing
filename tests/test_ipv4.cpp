#include "check.h"
#include "ipv4.h"

int main() {
  // addresses
  CHECK(parseAddress("192.168.1.5") == 0xC0A80105u);
  CHECK(parseAddress("0.0.0.0") == 0u);
  CHECK(parseAddress("255.255.255.255") == 0xFFFFFFFFu);
  for (const char *bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.-4", "1..3.4", "1.2.3.4 ", " 1.2.3.4",
                          "1.2.3.4/24", "a.b.c.d", "1.2.3.0004", "+1.2.3.4"})
    CHECK(!parseAddress(bad));

  // prefixes: host bits are cleared
  auto p = parsePrefix("192.168.20.16/28");
  CHECK(p && p->addr == 0xC0A81410u && p->len == 28);
  p = parsePrefix("10.10.3.4/16");
  CHECK(p && p->addr == 0x0A0A0000u && p->len == 16);
  p = parsePrefix("0.0.0.0/0");
  CHECK(p && p->addr == 0u && p->len == 0);
  p = parsePrefix("8.8.8.8/32");
  CHECK(p && p->addr == 0x08080808u && p->len == 32);
  for (const char *bad : {"10.0.0.0", "10.0.0.0/", "10.0.0.0/33", "10.0.0.0/-1", "10.0.0.0/8/8", "10.0.0/8",
                          "10.0.0.0/8x", "10.0.0.0/008"})
    CHECK(!parsePrefix(bad));

  // masks and containment
  CHECK_EQ(mask(0), 0u);
  CHECK_EQ(mask(1), 0x80000000u);
  CHECK_EQ(mask(24), 0xFFFFFF00u);
  CHECK_EQ(mask(32), 0xFFFFFFFFu);
  CHECK(contains(*parsePrefix("192.168.20.16/28"), *parseAddress("192.168.20.31")));
  CHECK(!contains(*parsePrefix("192.168.20.16/28"), *parseAddress("192.168.20.32")));
  CHECK(contains(*parsePrefix("0.0.0.0/0"), *parseAddress("8.8.8.8")));

  // formatting
  CHECK_EQ(formatAddress(0xC0A80105u), "192.168.1.5");
  CHECK_EQ(formatPrefix(*parsePrefix("10.10.3.4/16")), "10.10.0.0/16");

  return report("test_ipv4");
}
