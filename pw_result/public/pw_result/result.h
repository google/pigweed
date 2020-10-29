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

#include "pw_assert/light.h"
#include "pw_status/status.h"

namespace pw {

// A Result represents the result of an operation which can fail. It is a
// convenient wrapper around returning a Status alongside some data when the
// status is OK.
template <typename T>
class Result {
 public:
  constexpr Result(T&& value) : value_(std::move(value)), status_(OkStatus()) {}
  constexpr Result(const T& value) : value_(value), status_(OkStatus()) {}

  template <typename... Args>
  constexpr Result(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), status_(OkStatus()) {}

  constexpr Result(Status status) : dummy_({}), status_(status) {
    PW_ASSERT(!status_.ok());
  }
  constexpr Result(Status::Code code) : dummy_({}), status_(code) {
    PW_ASSERT(!status_.ok());
  }

  constexpr Result(const Result&) = default;
  constexpr Result& operator=(const Result&) = default;

  constexpr Result(Result&&) = default;
  constexpr Result& operator=(Result&&) = default;

  constexpr Status status() const { return status_; }
  constexpr bool ok() const { return status_.ok(); }

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

 private:
  struct Dummy {};

  union {
    T value_;

    // Ensure that there is always a trivial constructor for the union.
    Dummy dummy_;
  };
  Status status_;
};

}  // namespace pw
