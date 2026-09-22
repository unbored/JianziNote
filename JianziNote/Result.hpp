// Copyright (c) 2026 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace qin {

enum class JianziErrorCode {
  IoError,
  InvalidCbor,
  InvalidLibrary,
  InvalidUtf8,
  InvalidFormula,
  AliasCycle,
  InvalidStrokeDesc,
  InvalidGeometry,
  ContextMismatch,
};

struct JianziError {
  JianziErrorCode code;
  std::string message;
};

// C++17 replacement for std::expected<T, JianziError>. Accessors return
// pointers so that inspecting the wrong alternative never throws.
template <typename T>
class [[nodiscard]] Result {
 public:
  static Result Success(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    return Result(SuccessTag{}, std::move(value));
  }

  static Result Failure(JianziError error) noexcept(std::is_nothrow_move_constructible_v<JianziError>) {
    return Result(FailureTag{}, std::move(error));
  }

  bool HasValue() const noexcept { return m_storage.index() == 0; }
  explicit operator bool() const noexcept { return HasValue(); }

  T* GetValue() noexcept { return std::get_if<0>(&m_storage); }
  const T* GetValue() const noexcept { return std::get_if<0>(&m_storage); }

  JianziError* GetError() noexcept { return std::get_if<1>(&m_storage); }
  const JianziError* GetError() const noexcept { return std::get_if<1>(&m_storage); }

 private:
  struct SuccessTag {};
  struct FailureTag {};

  explicit Result(SuccessTag, T value) noexcept(std::is_nothrow_move_constructible_v<T>)
      : m_storage(std::in_place_index<0>, std::move(value)) {}

  explicit Result(FailureTag, JianziError error) noexcept(
      std::is_nothrow_move_constructible_v<JianziError>)
      : m_storage(std::in_place_index<1>, std::move(error)) {}

  std::variant<T, JianziError> m_storage;
};

template <>
class [[nodiscard]] Result<void> {
 public:
  static Result Success() noexcept { return Result(SuccessTag{}); }

  static Result Failure(JianziError error) noexcept(std::is_nothrow_move_constructible_v<JianziError>) {
    return Result(FailureTag{}, std::move(error));
  }

  bool HasValue() const noexcept { return m_storage.index() == 0; }
  explicit operator bool() const noexcept { return HasValue(); }

  JianziError* GetError() noexcept { return std::get_if<1>(&m_storage); }
  const JianziError* GetError() const noexcept { return std::get_if<1>(&m_storage); }

 private:
  struct SuccessTag {};
  struct FailureTag {};

  explicit Result(SuccessTag) noexcept : m_storage(std::in_place_index<0>) {}

  explicit Result(FailureTag, JianziError error) noexcept(
      std::is_nothrow_move_constructible_v<JianziError>)
      : m_storage(std::in_place_index<1>, std::move(error)) {}

  std::variant<std::monostate, JianziError> m_storage;
};

}  // namespace qin
