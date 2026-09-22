// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Utf8.hpp"

#include <iterator>

#include <utf8.h>

namespace qin::utf8 {

Result<std::u32string> Decode(std::string_view input) {
  if (!::utf8::is_valid(input.begin(), input.end())) {
    return Result<std::u32string>::Failure(
        {JianziErrorCode::InvalidUtf8, "Invalid UTF-8 input."});
  }
  std::u32string result;
  result.reserve(input.size());
  ::utf8::unchecked::utf8to32(input.begin(), input.end(), std::back_inserter(result));
  return Result<std::u32string>::Success(std::move(result));
}

Result<std::string> Encode(std::u32string_view input) {
  for (const auto codepoint : input) {
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
      return Result<std::string>::Failure(
          {JianziErrorCode::InvalidUtf8, "Invalid Unicode code point."});
    }
  }
  std::string result;
  result.reserve(input.size() * 4);
  ::utf8::unchecked::utf32to8(input.begin(), input.end(), std::back_inserter(result));
  return Result<std::string>::Success(std::move(result));
}

}  // namespace qin::utf8
