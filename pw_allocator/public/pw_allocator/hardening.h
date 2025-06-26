// Copyright 2024 The Pigweed Authors
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

#include "pw_allocator/config.h"
#include "pw_assert/assert.h"
#include "pw_numeric/checked_arithmetic.h"

namespace pw::allocator {

/// Configuration that determines which validation checks will performed.
///
/// This purely-static type provides both symbols that can be used to
/// conditionally exclude unwanted checks, as well as common arithmetic routines
/// that can perform overflow checking when configured to do so.
///
/// The behavior of this type is determined by the `PW_ALLOCATOR_HARDENING`
/// module configuration option.
struct Hardening {
  enum Checks {
    kBasic = PW_ALLOCATOR_HARDENING_BASIC,
    kRobust = PW_ALLOCATOR_HARDENING_ROBUST,
    kDebug = PW_ALLOCATOR_HARDENING_DEBUG,
  };

  static constexpr bool kIncludesBasicChecks =
      PW_ALLOCATOR_HARDENING >= PW_ALLOCATOR_HARDENING_BASIC;

  static constexpr bool kIncludesRobustChecks =
      PW_ALLOCATOR_HARDENING >= PW_ALLOCATOR_HARDENING_ROBUST;

  static constexpr bool kIncludesDebugChecks =
      PW_ALLOCATOR_HARDENING >= PW_ALLOCATOR_HARDENING_DEBUG;

  template <typename T, typename U>
  static constexpr void Increment(T& value, U increment) {
    if constexpr (Hardening::kIncludesRobustChecks) {
      PW_ASSERT(CheckedIncrement(value, increment));
    } else {
      value += increment;
    }
  }

  template <typename T, typename U>
  static constexpr void Decrement(T& value, U increment) {
    if constexpr (Hardening::kIncludesRobustChecks) {
      PW_ASSERT(CheckedDecrement(value, increment));
    } else {
      value -= increment;
    }
  }

  template <typename T, typename U>
  static constexpr void Multiply(T& value, U increment) {
    if constexpr (Hardening::kIncludesRobustChecks) {
      PW_ASSERT(CheckedMul(value, increment, value));
    } else {
      value *= increment;
    }
  }
};

}  // namespace pw::allocator
