// Copyright 2020 The Pigweed Authors
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

#include <algorithm>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/status.h"

namespace pw {

// A Result represents the result of an operation which can fail. It is a
// convenient wrapper around returning a Status alongside some data when the
// status is OK.
//
// TODO(pwbug/363): Refactor pw::Result to properly support non-default move
// and/or copy assignment operators and/or constructors.
template <typename T>
class [[nodiscard]] Result {
 public:
  constexpr Result(T&& value) : value_(std::move(value)), status_(OkStatus()) {}
  constexpr Result(const T& value) : value_(value), status_(OkStatus()) {}

  template <typename... Args>
  constexpr Result(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), status_(OkStatus()) {}

  constexpr Result(Status status) : unused_({}), status_(status) {
    PW_ASSERT(!status_.ok());
  }
  constexpr Result(Status::Code code) : unused_({}), status_(code) {
    PW_ASSERT(!status_.ok());
  }

  constexpr Result(const Result&) = default;
  constexpr Result& operator=(const Result&) = default;

  constexpr Result(Result&&) = default;
  constexpr Result& operator=(Result&&) = default;

  [[nodiscard]] constexpr Status status() const { return status_; }
  [[nodiscard]] constexpr bool ok() const { return status_.ok(); }

  constexpr T& value() & {
    PW_ASSERT(status_.ok());
    return value_;
  }

  constexpr const T& value() const& {
    PW_ASSERT(status_.ok());
    return value_;
  }

  constexpr T&& value() && {
    PW_ASSERT(status_.ok());
    return std::move(value_);
  }

  constexpr T& operator*() const& {
    PW_ASSERT(status_.ok());
    return value_;
  }

  T& operator*() & {
    PW_ASSERT(status_.ok());
    return value_;
  }

  constexpr T&& operator*() const&& {
    PW_ASSERT(status_.ok());
    return std::move(value_);
  }

  T&& operator*() && {
    PW_ASSERT(status_.ok());
    return std::move(value_);
  }

  constexpr T* operator->() const {
    PW_ASSERT(status_.ok());
    return &value_;
  }

  T* operator->() {
    PW_ASSERT(status_.ok());
    return &value_;
  }

  template <typename U>
  constexpr T value_or(U&& default_value) const& {
    if (ok()) {
      PW_MODIFY_DIAGNOSTICS_PUSH();
      // GCC 10 emits -Wmaybe-uninitialized warnings about value_.
      PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wmaybe-uninitialized");
      return value_;
      PW_MODIFY_DIAGNOSTICS_POP();
    }
    return std::forward<U>(default_value);
  }

  template <typename U>
  constexpr T value_or(U&& default_value) && {
    if (ok()) {
      return std::move(value_);
    }
    return std::forward<U>(default_value);
  }

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  constexpr void IgnoreError() const {}

 private:
  struct Unused {};

  union {
    T value_;

    // Ensure that there is always a trivial constructor for the union.
    Unused unused_;
  };
  Status status_;
};

namespace internal {

template <typename T>
constexpr Status ConvertToStatus(const Result<T>& result) {
  return result.status();
}

template <typename T>
constexpr T ConvertToValue(Result<T>& result) {
  return std::move(result.value());
}

}  // namespace internal
}  // namespace pw
