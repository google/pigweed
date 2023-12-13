// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <optional>

namespace pw::async2 {

/// A type whose value indicates that an operation was able to complete (or
/// was ready to produce an output).
///
/// This type is used as the contentless "value" type for ``Poll``
/// types that do not return a value.
struct ReadyType {};

// nodiscard with a string literal is only available starting in C++20.
#if __cplusplus >= 202002L
#define PW_NODISCARD_STR(str) [[nodiscard(str)]]
#else
#define PW_NODISCARD_STR(str) [[nodiscard]]
#endif

/// A type whose value indicates an operation was not yet able to complete.
///
/// This is analogous to ``std::nullopt_t``, but for ``Poll``.
struct PW_NODISCARD_STR(
    "`Poll`-returning functions may or may not have completed. Their "
    "return "
    "value should be examined.") PendingType {};

template <typename T = ReadyType>
class PW_NODISCARD_STR(
    "`Poll`-returning functions may or may not have completed. Their "
    "return "
    "value should be examined.") Poll {
 public:
  /// Basic constructors.
  Poll() = delete;
  constexpr Poll(const Poll&) = default;
  constexpr Poll& operator=(const Poll&) = default;
  constexpr Poll(Poll&&) = default;
  constexpr Poll& operator=(Poll&&) = default;

  // In-place construction of ``Ready`` variant.
  template <typename... Args>
  constexpr Poll(std::in_place_t, Args&&... args)
      : value_(std::in_place, std::move(args)...) {}

  // Convert from `T`
  constexpr Poll(T&& value) : value_(std::move(value)) {}
  constexpr Poll& operator=(T&& value) {
    value_ = std::optional<T>(std::move(value));
    return *this;
  }

  // Convert from `Pending`
  constexpr Poll(PendingType) noexcept : value_() {}
  constexpr Poll& operator=(PendingType) noexcept {
    value_ = std::nullopt;
    return *this;
  }

  /// Returns whether or not this value is ``Ready``.
  constexpr bool IsReady() const noexcept { return value_.has_value(); }

  /// Returns the inner value.
  ///
  /// This must only be called if ``IsReady()`` returned ``true``.
  constexpr T& value() & noexcept { return *value_; }
  constexpr const T& value() const& noexcept { return *value_; }
  constexpr T&& value() && noexcept { return std::move(*value_); }
  constexpr const T&& value() const&& noexcept { return std::move(*value_); }

  /// Accesses the inner value.
  ///
  /// This must only be called if ``IsReady()`` returned ``true``.
  constexpr const T* operator->() const noexcept { return &*value_; }
  constexpr T* operator->() noexcept { return &*value_; }

  /// Returns the inner value.
  ///
  /// This must only be called if ``IsReady()`` returned ``true``.
  constexpr const T& operator*() const& noexcept { return *value_; }
  constexpr T& operator*() & noexcept { return *value_; }
  constexpr const T&& operator*() const&& noexcept {
    return std::move(*value_);
  }
  constexpr T&& operator*() && noexcept { return std::move(*value_); }

 private:
  std::optional<T> value_;
};

/// Returns a value indicating completion.
inline constexpr Poll<> Ready() { return Poll(ReadyType{}); }

/// Returns a value indicating completion with some result
/// (constructed in-place).
template <typename T, typename... Args>
constexpr Poll<T> Ready(std::in_place_t, Args&&... args) {
  return Poll<T>(std::in_place, std::forward<Args>(args)...);
}

/// Returns a value indicating completion with some result.
template <typename T>
constexpr Poll<T> Ready(T&& value) {
  return Poll<T>(std::forward<T>(value));
}

/// Returns a value indicating that an operation was not yet able to complete.
inline constexpr PendingType Pending() { return PendingType(); }

#undef PW_NODISCARD_STR

}  // namespace pw::async2
