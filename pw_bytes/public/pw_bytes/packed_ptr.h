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

#include "lib/stdcompat/bit.h"
#include "pw_assert/assert.h"

namespace pw {

/// @submodule{pw_bytes,ptr}

/// Pointer wrapper that can store extra data in otherwise unused bits.
///
/// Pointers to types that have an alignment of more than 1 will always have
/// their least significant bit(s) set to zero. For example, if `alignof(T)` is
/// 8, than the 3 least significant bits are always 0. In certain contexts where
/// overhead needs to be tightly constrained, reusing these bits and avoiding
/// additional fields may be worth the performance and code size penalties that
/// arise from masking values.
template <typename T>
class PackedPtr {
 public:
  // Calculate the number of available bits in a function, since the type T may
  // include fields of type `PackedPtr<T>`  and not be fully defined when the
  // `PackedPtr<T>` class is instantiated.
  static constexpr size_t NumBits() {
    CheckAlignment();
    return 31 - cpp20::countl_zero(static_cast<uint32_t>(alignof(T)));
  }

  constexpr explicit PackedPtr() { CheckAlignment(); }
  ~PackedPtr() = default;

  constexpr PackedPtr(T* ptr, uintptr_t packed_value) : PackedPtr() {
    set(ptr);
    set_packed_value(packed_value);
  }

  template <typename T2,
            typename = std::enable_if_t<std::is_convertible_v<T2, T>>>
  constexpr PackedPtr(const PackedPtr<T2>& other) {
    *this = other;
  }

  template <typename T2,
            typename = std::enable_if_t<std::is_convertible_v<T2, T>>>
  constexpr PackedPtr& operator=(const PackedPtr<T2>& other) {
    data_ = other.data_;
    return *this;
  }

  template <typename T2,
            typename = std::enable_if_t<std::is_convertible_v<T2, T>>>
  constexpr PackedPtr(PackedPtr<T2>&& other) {
    *this = std::move(other);
  }

  template <typename T2,
            typename = std::enable_if_t<std::is_convertible_v<T2, T>>>
  constexpr PackedPtr& operator=(PackedPtr<T2>&& other) {
    data_ = std::exchange(other.data_, 0);
    return *this;
  }

  constexpr T& operator*() const { return *(get()); }
  constexpr T* operator->() const { return get(); }

  /// Returns the pointer.
  constexpr T* get() const { return cpp20::bit_cast<T*>(data_ & ~kValueMask); }

  /// Returns the packed_value packed into the unused bits of the pointer.
  constexpr uintptr_t packed_value() const { return data_ & kValueMask; }

  /// Sets the pointer without changing the packed value.
  constexpr void set(T* ptr) {
    auto packed_ptr = cpp20::bit_cast<uintptr_t, T*>(ptr);
    PW_DASSERT((packed_ptr & kValueMask) == 0);
    data_ = packed_ptr | packed_value();
  }

  /// Packs a packed_value into the unused bits of the pointer.
  ///
  /// @pre The given packed_value must fit in the available bits.
  constexpr void set_packed_value(uintptr_t packed_value) {
    PW_DASSERT(packed_value <= kValueMask);
    data_ = (data_ & ~kValueMask) | packed_value;
  }

 private:
  // Check the alignment of T in a function, since the type T may include fields
  // of type `PackedPtr<T>`  and not be fully defined when the `PackedPtr<T>`
  // class is instantiated.
  static constexpr void CheckAlignment() {
    static_assert(alignof(T) > 1,
                  "Alignment must be more than one to pack any bits");
  }

  static constexpr uintptr_t kValueMask = (1 << NumBits()) - 1;

  // Allows copying and moving between convertible types.
  template <typename>
  friend class PackedPtr;

  uintptr_t data_ = 0;
};

/// @}

}  // namespace pw
