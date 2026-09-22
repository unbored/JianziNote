// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Cbor.hpp"

namespace qin::cbor {

Result<nlohmann::json> Read(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) {
    return Result<nlohmann::json>::Failure(
        {JianziErrorCode::InvalidCbor, "CBOR input is empty."});
  }

  auto document = nlohmann::json::from_cbor(data, data + size, true, false);
  if (document.is_discarded()) {
    return Result<nlohmann::json>::Failure(
        {JianziErrorCode::InvalidCbor, "Invalid CBOR document."});
  }
  return Result<nlohmann::json>::Success(std::move(document));
}

}  // namespace qin::cbor
