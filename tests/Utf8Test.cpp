#include <string>

#include "Utf8.hpp"

int main() {
  const std::string sample = u8"大𠀀😀";
  const auto decoded = qin::utf8::Decode(sample);
  if (!decoded || decoded.GetValue()->size() != 3) return 1;
  const auto encoded = qin::utf8::Encode(*decoded.GetValue());
  if (!encoded || *encoded.GetValue() != sample) return 2;

  const auto invalid_utf8 = qin::utf8::Decode(std::string("\xC0\xAF", 2));
  if (invalid_utf8 || invalid_utf8.GetError()->code != qin::JianziErrorCode::InvalidUtf8) return 3;

  const auto invalid_codepoint =
      qin::utf8::Encode(std::u32string(1, static_cast<char32_t>(0xD800)));
  return !invalid_codepoint &&
                 invalid_codepoint.GetError()->code == qin::JianziErrorCode::InvalidUtf8
             ? 0
             : 4;
}
