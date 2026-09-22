// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "Result.hpp"

namespace qin::cbor {

// Decode one complete CBOR document from memory. The input buffer only needs
// to remain valid for the duration of this call.
Result<nlohmann::json> Read(const std::uint8_t* data, std::size_t size);

}  // namespace qin::cbor
