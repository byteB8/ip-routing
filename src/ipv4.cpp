#include "ipv4.h"

#include <charconv>

namespace {

// Reads a decimal number of 1 to maxDigits digits from the front of text and removes it.
std::optional<unsigned> takeNumber(std::string_view &text, size_t maxDigits) {
  unsigned value = 0;
  auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  size_t digits = end - text.data();
  if (ec != std::errc() || digits == 0 || digits > maxDigits)
    return std::nullopt;
  text.remove_prefix(digits);
  return value;
}

}  // namespace

std::optional<uint32_t> parseAddress(std::string_view text) {
  uint32_t addr = 0;
  for (int i = 0; i < 4; ++i) {
    if (i > 0) {
      if (text.empty() || text.front() != '.')
        return std::nullopt;
      text.remove_prefix(1);
    }
    auto octet = takeNumber(text, 3);
    if (!octet || *octet > 255)
      return std::nullopt;
    addr = (addr << 8) | *octet;
  }
  if (!text.empty())
    return std::nullopt;
  return addr;
}

std::optional<Prefix> parsePrefix(std::string_view text) {
  size_t slash = text.find('/');
  if (slash == std::string_view::npos)
    return std::nullopt;
  auto addr = parseAddress(text.substr(0, slash));
  std::string_view lenText = text.substr(slash + 1);
  auto len = takeNumber(lenText, 2);
  if (!addr || !len || *len > 32 || !lenText.empty())
    return std::nullopt;
  int bits = static_cast<int>(*len);
  return Prefix{*addr & mask(bits), bits};
}

std::string formatAddress(uint32_t addr) {
  return std::to_string(addr >> 24) + '.' + std::to_string((addr >> 16) & 0xFF) + '.' +
         std::to_string((addr >> 8) & 0xFF) + '.' + std::to_string(addr & 0xFF);
}

std::string formatPrefix(const Prefix &p) { return formatAddress(p.addr) + '/' + std::to_string(p.len); }
