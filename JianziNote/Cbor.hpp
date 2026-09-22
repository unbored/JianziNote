// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace qin::cbor {

struct Error {
  std::string message;
};

struct ReadResult {
  nlohmann::json document;
  std::optional<Error> error;

  explicit operator bool() const { return !error.has_value(); }
};

// Decode one complete CBOR document from memory. The input buffer only needs
// to remain valid for the duration of this call.
ReadResult Read(const std::uint8_t* data, std::size_t size);

}  // namespace qin::cbor
