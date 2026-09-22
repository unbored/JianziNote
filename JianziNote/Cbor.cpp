// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Cbor.hpp"

namespace qin::cbor {

ReadResult Read(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) {
    return {{}, Error{"CBOR input is empty."}};
  }

  try {
    return {nlohmann::json::from_cbor(data, data + size), std::nullopt};
  } catch (const std::exception& error) {
    return {{}, Error{std::string("Invalid CBOR document: ") + error.what()}};
  }
}

}  // namespace qin::cbor
