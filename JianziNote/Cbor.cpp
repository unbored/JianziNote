// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Cbor.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

namespace qin::cbor {

ReadResult Read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {{}, Error{"Unable to open CBOR file for reading.", path}};
  }

  std::vector<std::uint8_t> bytes;
  bytes.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
  if (input.bad()) {
    return {{}, Error{"Unable to read CBOR file.", path}};
  }

  try {
    return {nlohmann::json::from_cbor(bytes), std::nullopt};
  } catch (const std::exception& error) {
    return {{}, Error{std::string("Invalid CBOR document: ") + error.what(),
                      path}};
  }
}

std::optional<Error> Write(const std::filesystem::path& path,
                           const nlohmann::json& document) {
  try {
    const auto bytes = nlohmann::json::to_cbor(document);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
      return Error{"Unable to open CBOR file for writing.", path};
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      return Error{"Unable to write CBOR file.", path};
    }
  } catch (const std::exception& error) {
    return Error{std::string("Unable to encode CBOR document: ") +
                     error.what(),
                 path};
  }
  return std::nullopt;
}

}  // namespace qin::cbor
