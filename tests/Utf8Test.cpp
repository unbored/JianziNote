#include <stdexcept>
#include <string>

#include "Utf8.hpp"

int main() {
  const std::string sample = u8"大𠀀😀";
  const auto decoded = qin::utf8::Decode(sample);
  if (decoded.size() != 3 || qin::utf8::Encode(decoded) != sample) return 1;

  bool rejected = false;
  try {
    static_cast<void>(qin::utf8::Decode(std::string("\xC0\xAF", 2)));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  if (!rejected) return 2;

  rejected = false;
  try {
    static_cast<void>(qin::utf8::Encode(std::u32string(1, static_cast<char32_t>(0xD800))));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  return rejected ? 0 : 3;
}
