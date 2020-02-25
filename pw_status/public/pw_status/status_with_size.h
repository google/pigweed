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

#include <cstddef>
#include <type_traits>

#include "pw_status/status.h"

namespace pw {

class StatusWithSize;

namespace internal {

template <int kStatusShift>
class StatusWithSizeConstant {
 private:
  friend class ::pw::StatusWithSize;

  explicit constexpr StatusWithSizeConstant(Status::Code value)
      : value_(static_cast<size_t>(value) << kStatusShift) {}

  const size_t value_;
};

}  // namespace internal

// StatusWithSize stores a status and an unsigned integer. The integer must not
// exceed StatusWithSize::max_size(), which is 134,217,727 (2**27 - 1) on 32-bit
// systems.
//
// StatusWithSize is useful for reporting the number of bytes read or written in
// an operation along with the status. For example, a function that writes a
// formatted string may want to report both the number of characters written and
// whether it ran out of space.
//
// StatusWithSize is more efficient than its alternatives. It packs a status and
// size into a single word, which can be returned from a function in a register.
// Because they are packed together, the size is limited to max_size().
//
// StatusWithSize's alternatives result in larger code size. For example:
//
//   1. Return status, pass size output as a pointer argument.
//
//      Requires an additional argument and forces the output argument to the
//      stack in order to pass an address, increasing code size.
//
//   2. Return an object with Status and size members.
//
//      At least for ARMv7-M, the returned struct is created on the stack, which
//      increases code size.
//
class StatusWithSize {
 private:
  static constexpr size_t kStatusBits = 5;
  static constexpr size_t kSizeMask = ~static_cast<size_t>(0) >> kStatusBits;
  static constexpr size_t kStatusMask = ~kSizeMask;
  static constexpr size_t kStatusShift = sizeof(size_t) * 8 - kStatusBits;

  using Constant = internal::StatusWithSizeConstant<kStatusShift>;

 public:
  // Non-OK StatusWithSizes can be constructed from these constants, such as:
  //
  //   StatusWithSize result = StatusWithSize::NOT_FOUND;
  //
  static constexpr Constant CANCELLED{Status::CANCELLED};
  static constexpr Constant UNKNOWN{Status::UNKNOWN};
  static constexpr Constant INVALID_ARGUMENT{Status::INVALID_ARGUMENT};
  static constexpr Constant DEADLINE_EXCEEDED{Status::DEADLINE_EXCEEDED};
  static constexpr Constant NOT_FOUND{Status::NOT_FOUND};
  static constexpr Constant ALREADY_EXISTS{Status::ALREADY_EXISTS};
  static constexpr Constant PERMISSION_DENIED{Status::PERMISSION_DENIED};
  static constexpr Constant RESOURCE_EXHAUSTED{Status::RESOURCE_EXHAUSTED};
  static constexpr Constant FAILED_PRECONDITION{Status::FAILED_PRECONDITION};
  static constexpr Constant ABORTED{Status::ABORTED};
  static constexpr Constant OUT_OF_RANGE{Status::OUT_OF_RANGE};
  static constexpr Constant UNIMPLEMENTED{Status::UNIMPLEMENTED};
  static constexpr Constant INTERNAL{Status::INTERNAL};
  static constexpr Constant UNAVAILABLE{Status::UNAVAILABLE};
  static constexpr Constant DATA_LOSS{Status::DATA_LOSS};
  static constexpr Constant UNAUTHENTICATED{Status::UNAUTHENTICATED};

  // Creates a StatusWithSize with Status::OK and a size of 0.
  explicit constexpr StatusWithSize() : size_(0) {}

  // Creates a StatusWithSize with Status::OK and the provided size.
  // std::enable_if is used to prevent enum types (e.g. Status) from being used.
  // TODO(hepler): Add debug-only assert that size <= max_size().
  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  explicit constexpr StatusWithSize(T size) : size_(size) {}

  // Creates a StatusWithSize with the provided status and size.
  constexpr StatusWithSize(Status::Code status, size_t size)
      : StatusWithSize((static_cast<size_t>(status) << kStatusShift) | size) {}

  // Allow implicit conversions from the StatusWithSize constants.
  constexpr StatusWithSize(Constant constant) : size_(constant.value_) {}

  constexpr StatusWithSize(const StatusWithSize&) = default;
  constexpr StatusWithSize& operator=(const StatusWithSize&) = default;

  // Returns the size. The size is always present, even if status() is an error.
  constexpr size_t size() const { return size_ & kSizeMask; }

  // The maximum valid value for size.
  static constexpr size_t max_size() { return kSizeMask; }

  // True if status() == Status::OK.
  constexpr bool ok() const { return (size_ & kStatusMask) == 0u; }

  constexpr Status status() const {
    return static_cast<Status::Code>((size_ & kStatusMask) >> kStatusShift);
  }

 private:
  size_t size_;
};

}  // namespace pw
