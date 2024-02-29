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

#include "pw_async2/internal/poll_internal.h"

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

/// A value that may or may not be ready yet.
///
/// ``Poll<T>`` most commonly appears as the return type of an function
/// that checks the current status of an asynchronous operation. If
/// the operation has completed, it returns with ``Ready(value)``. Otherwise,
/// it returns ``Pending`` to indicate that the operations has not yet
/// completed, and the caller should try again in the future.
///
/// ``Poll<T>`` itself is "plain old data" and does not change on its own.
/// To check the current status of an operation, the caller must invoke
/// the ``Poll<T>`` returning function again and examine the newly returned
/// ``Poll<T>``.
template <typename T = ReadyType>
class PW_NODISCARD_STR(
    "`Poll`-returning functions may or may not have completed. Their "
    "return value should be examined.") Poll {
 public:
  /// Basic constructors.
  Poll() = delete;
  constexpr Poll(const Poll&) = default;
  constexpr Poll& operator=(const Poll&) = default;
  constexpr Poll(Poll&&) = default;
  constexpr Poll& operator=(Poll&&) = default;

  /// Constructs a new ``Poll<T>`` from a ``Poll<U>`` where ``T`` is
  /// constructible from ``U``.
  ///
  /// To avoid ambiguity, this constructor is disabled if ``T`` is also
  /// constructible from ``Poll<U>``.
  ///
  /// This constructor is explicit if and only if the corresponding construction
  /// of ``T`` from ``U`` is explicit.
  template <typename U,
            internal_poll::EnableIfImplicitlyConvertible<T, const U&> = 0>
  constexpr Poll(const Poll<U>& other) : value_(other.value_) {}
  template <typename U,
            internal_poll::EnableIfExplicitlyConvertible<T, const U&> = 0>
  explicit constexpr Poll(const Poll<U>& other) : value_(other.value_) {}

  template <typename U,
            internal_poll::EnableIfImplicitlyConvertible<T, U&&> = 0>
  constexpr Poll(Poll<U>&& other)  // NOLINT
      : value_(std::move(other.value_)) {}
  template <typename U,
            internal_poll::EnableIfExplicitlyConvertible<T, U&&> = 0>
  explicit constexpr Poll(Poll<U>&& other) : value_(std::move(other.value_)) {}

  // Constructs the inner value `T` in-place using the provided args, using the
  // `T(U)` (direct-initialization) constructor. This constructor is only valid
  // if `T` can be constructed from a `U`. Can accept move or copy constructors.
  //
  // This constructor is explicit if `U` is not convertible to `T`. To avoid
  // ambiguity, this constructor is disabled if `U` is a `Poll<J>`, where
  // `J` is convertible to `T`.
  template <typename U = T,
            internal_poll::EnableIfImplicitlyInitializable<T, U> = 0>
  constexpr Poll(U&& u)  // NOLINT
      : Poll(std::in_place, std::forward<U>(u)) {}

  template <typename U = T,
            internal_poll::EnableIfExplicitlyInitializable<T, U> = 0>
  explicit constexpr Poll(U&& u)  // NOLINT
      : Poll(std::in_place, std::forward<U>(u)) {}

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
  template <typename U>
  friend class Poll;
  std::optional<T> value_;
};

// Deduction guide to allow ``Poll(v)`` rather than ``Poll<T>(v)``.
template <typename T>
Poll(T value) -> Poll<T>;

/// Returns whether two instances of ``Poll<T>`` are equal.
///
/// Note that this comparison operator will return ``true`` if both
/// values are currently ``Pending``, even if the eventual results
/// of each operation might differ.
template <typename T>
constexpr bool operator==(const Poll<T>& lhs, const Poll<T>& rhs) {
  if (lhs.IsReady() && rhs.IsReady()) {
    return *lhs == *rhs;
  }
  return lhs.IsReady() == rhs.IsReady();
}

/// Returns whether two instances of ``Poll<T>`` are unequal.
///
/// Note that this comparison operator will return ``false`` if both
/// values are currently ``Pending``, even if the eventual results
/// of each operation might differ.
template <typename T>
constexpr bool operator!=(const Poll<T>& lhs, const Poll<T>& rhs) {
  return !(lhs == rhs);
}

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
