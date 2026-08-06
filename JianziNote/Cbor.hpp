// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace qin::cbor {

struct Error {
  std::string message;
  std::filesystem::path path;
};

struct ReadResult {
  nlohmann::json document;
  std::optional<Error> error;

  explicit operator bool() const { return !error.has_value(); }
};

// Read one complete CBOR document from a file. This class deliberately knows
// nothing about Jianzi's data schema; schema validation belongs to its caller.
ReadResult Read(const std::filesystem::path& path);

// Write one complete CBOR document to a file. Parent directories must already
// exist so that callers retain control of their output layout.
std::optional<Error> Write(const std::filesystem::path& path,
                           const nlohmann::json& document);

}  // namespace qin::cbor
