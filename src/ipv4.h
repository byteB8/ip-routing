#ifndef IPV4_H
#define IPV4_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// An IPv4 prefix: the network address (host bits cleared) and its length in bits, 0 to 32.
struct Prefix {
  uint32_t addr = 0;
  int len = 0;
};

// Network mask of a prefix length: mask(24) == 0xFFFFFF00, mask(0) == 0.
constexpr uint32_t mask(int len) { return len == 0 ? 0u : ~0u << (32 - len); }

// True if the address lies inside the prefix.
constexpr bool contains(const Prefix &p, uint32_t addr) { return (addr & mask(p.len)) == p.addr; }

std::optional<uint32_t> parseAddress(std::string_view text);  // "192.168.1.5"
std::optional<Prefix> parsePrefix(std::string_view text);     // "192.168.1.0/24"; host bits are cleared
std::string formatAddress(uint32_t addr);
std::string formatPrefix(const Prefix &p);

#endif
