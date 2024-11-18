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

#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::allocator {

/// This class communicates the results and side effects of allocator requests
/// by compactly combining a typical `Status` with enumerated values describing
/// how a block's previous and next neighboring blocks may have been changed.
class [[nodiscard]] BlockResult {
 public:
  enum class Prev : uint8_t {
    kUnchanged,
    kSplitNew,
    kResized,
  };

  enum class Next : uint8_t {
    kUnchanged,
    kSplitNew,
    kResized,
    kMerged,
  };

  constexpr explicit BlockResult() : BlockResult(pw::OkStatus()) {}
  constexpr explicit BlockResult(Status status)
      : BlockResult(status, Prev::kUnchanged, Next::kUnchanged) {}
  constexpr explicit BlockResult(Prev prev)
      : BlockResult(OkStatus(), prev, Next::kUnchanged) {}
  constexpr explicit BlockResult(Next next)
      : BlockResult(OkStatus(), Prev::kUnchanged, next) {}
  constexpr explicit BlockResult(Prev prev, Next next)
      : BlockResult(OkStatus(), prev, next) {}

  [[nodiscard]] constexpr Prev prev() const {
    return static_cast<Prev>(encoded_.size() >> 8);
  }

  [[nodiscard]] constexpr Next next() const {
    return static_cast<Next>(encoded_.size() & 0xFF);
  }

  [[nodiscard]] constexpr bool ok() const { return encoded_.ok(); }

  [[nodiscard]] constexpr Status status() const { return encoded_.status(); }

  /// Asserts the result is not an error if strict validation is enabled, and
  /// does nothing otherwise.
  constexpr void IgnoreUnlessStrict() const {
    if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
      PW_ASSERT(ok());
    }
  }

 private:
  explicit constexpr BlockResult(Status status, Prev prev, Next next)
      : encoded_(status, (size_t(prev) << 8) | size_t(next)) {}

  StatusWithSize encoded_;
};

}  // namespace pw::allocator
