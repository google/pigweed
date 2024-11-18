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

#include <cstddef>
#include <cstdint>

#include "pw_allocator/config.h"
#include "pw_assert/assert.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::allocator {
namespace internal {

/// Generic base class for `BlockResult`.
///
/// This class communicates the results and side effects of allocator requests
/// by compactly combining a typical `Status` with enumerated values describing
/// how a block's previous and next neighboring blocks may have been changed.
class GenericBlockResult {
 private:
  static constexpr size_t kPrevBits = 5;
  static constexpr size_t kPrevShift = 0;

  static constexpr size_t kNextBits = 5;
  static constexpr size_t kNextShift = kPrevBits;

  static constexpr size_t kSizeBits = 10;
  static constexpr size_t kSizeShift = kPrevBits + kNextBits;

 public:
  enum class Prev : uint8_t {
    kUnchanged,
    kSplitNew,
    kResizedSmaller,
    kResizedLarger,
  };

  enum class Next : uint8_t {
    kUnchanged,
    kSplitNew,
    kResized,
    kMerged,
  };

  [[nodiscard]] constexpr Prev prev() const {
    return static_cast<Prev>(Decode(kPrevBits, kPrevShift));
  }

  [[nodiscard]] constexpr Next next() const {
    return static_cast<Next>(Decode(kNextBits, kNextShift));
  }

  [[nodiscard]] constexpr size_t size() const {
    return Decode(kSizeBits, kSizeShift);
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

 protected:
  constexpr GenericBlockResult(Status status, Prev prev, Next next, size_t size)
      : encoded_(status,
                 Encode(size_t(prev), kPrevBits, kPrevShift) |
                     Encode(size_t(next), kNextBits, kNextShift) |
                     Encode(size, kSizeBits, kSizeShift)) {}

 private:
  static constexpr size_t Encode(size_t value, size_t bits, size_t shift) {
    if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
      PW_ASSERT(value < (1U << bits));
    }
    return value << shift;
  }

  constexpr size_t Decode(size_t bits, size_t shift) const {
    return (encoded_.size() >> shift) & ~(~static_cast<size_t>(0) << bits);
  }

  StatusWithSize encoded_;
};

}  // namespace internal

/// Extends `GenericBlockResult` to include a pointer to a block.
///
/// The included pointer is to the block affected by the operation that produced
/// a result. On error, this should be the original block. On success, it may be
/// a newly produced block.
///
/// @tparam   BlockType   The type of the block included in the result.
template <typename BlockType>
class [[nodiscard]] BlockResult : public internal::GenericBlockResult {
 public:
  constexpr explicit BlockResult(BlockType* block)
      : BlockResult(block, OkStatus()) {}

  constexpr BlockResult(BlockType* block, Status status)
      : internal::GenericBlockResult(
            status, Prev::kUnchanged, Next::kUnchanged, 0),
        block_(block) {}

  constexpr BlockResult(BlockType* block, Prev prev)
      : BlockResult(block, prev, Next::kUnchanged, 0) {}

  constexpr BlockResult(BlockType* block, Prev prev, size_t shifted_to_prev)
      : BlockResult(block, prev, Next::kUnchanged, shifted_to_prev) {}

  constexpr BlockResult(BlockType* block, Next next)
      : BlockResult(block, Prev::kUnchanged, next, 0) {}

  constexpr BlockResult(BlockType* block, Prev prev, Next next)
      : BlockResult(block, prev, next, 0) {}

  constexpr BlockResult(BlockType* block,
                        Prev prev,
                        Next next,
                        size_t shifted_to_prev)
      : internal::GenericBlockResult(OkStatus(), prev, next, shifted_to_prev),
        block_(block) {
    block->CheckInvariantsIfStrict();
  }

  [[nodiscard]] constexpr BlockType* block() const { return block_; }

 private:
  BlockType* block_ = nullptr;
};

}  // namespace pw::allocator
