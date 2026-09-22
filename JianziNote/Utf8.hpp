// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <string_view>

#include "Result.hpp"

namespace qin::utf8 {

Result<std::u32string> Decode(std::string_view input);
Result<std::string> Encode(std::u32string_view input);

}  // namespace qin::utf8
