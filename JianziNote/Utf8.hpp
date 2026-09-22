// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <string_view>

namespace qin::utf8 {

std::u32string Decode(std::string_view input);
std::string Encode(std::u32string_view input);

}  // namespace qin::utf8
