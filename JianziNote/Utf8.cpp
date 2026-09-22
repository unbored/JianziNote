// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Utf8.hpp"

#include <iterator>
#include <stdexcept>

#include <utf8.h>

namespace qin::utf8 {

std::u32string Decode(std::string_view input) {
  std::u32string result;
  result.reserve(input.size());
  try {
    ::utf8::utf8to32(input.begin(), input.end(), std::back_inserter(result));
  } catch (const ::utf8::exception&) {
    throw std::invalid_argument("Invalid UTF-8 input.");
  }
  return result;
}

std::string Encode(std::u32string_view input) {
  std::string result;
  result.reserve(input.size());
  try {
    ::utf8::utf32to8(input.begin(), input.end(), std::back_inserter(result));
  } catch (const ::utf8::exception&) {
    throw std::invalid_argument("Invalid Unicode code point.");
  }
  return result;
}

}  // namespace qin::utf8
