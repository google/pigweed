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

#include <cstdint>

namespace pw::i2c {

/// A helper class that represents I2C addresses.
///
/// @code{.cpp}
///   #include "pw_i2c/address.h"
///
///   constexpr pw::i2c::Address kAddress1 = pw::i2c::Address::SevenBit<0x42>();
///   uint8_t raw_address_1 = kAddress1.GetSevenBit();
///
///   const pw::i2c::Address kAddress2(0x200);  // 10-bit
///   uint16_t raw_address_2 = kAddress2.GetTenBit();
///   // Note: kAddress2.GetSevenBit() would fail an assertion here.
/// @endcode
class Address {
 public:
  static constexpr uint8_t kMaxSevenBitAddress = (1 << 7) - 1;
  static constexpr uint16_t kMaxTenBitAddress = (1 << 10) - 1;

  /// Creates a `pw::i2c::Address` instance for an address that's 10 bits
  /// or less.
  ///
  /// This constant expression does a compile-time assertion to ensure that the
  /// provided address is 10 bits or less.
  ///
  /// @code{.cpp}
  ///   constexpr pw::i2c::Address kAddress = pw::i2c::Address::TenBit(0x200);
  /// @endcode
  ///
  /// @returns A `pw::i2c::Address` instance.
  template <uint16_t kAddress>
  static constexpr Address TenBit() {
    static_assert(kAddress <= kMaxTenBitAddress);
    return Address(kAddress, kAlreadyCheckedAddress);
  }

  /// Creates a `pw::i2c::Address` instance for an address that's 7 bits
  /// or less.
  ///
  /// This constant expression does a compile-time assertion to ensure that the
  /// provided address is 7 bits or less.
  ///
  /// @code{.cpp}
  ///   constexpr pw::i2c::Address kAddress = pw::i2c::Address::SevenBit(0x42);
  /// @endcode
  ///
  /// @returns A `pw::i2c::Address` instance.
  template <uint8_t kAddress>
  static constexpr Address SevenBit() {
    static_assert(kAddress <= kMaxSevenBitAddress);
    return Address(kAddress, kAlreadyCheckedAddress);
  }

  /// Creates a `pw::i2c::Address` instance.
  ///
  /// @param[in] address A 10-bit address as an unsigned integer. This method
  /// does a runtime assertion to ensure that `address` is 10 bits or less.
  ///
  /// @code{.cpp}
  ///   constexpr pw::i2c::Address kAddress(0x200);
  /// @endcode
  ///
  /// @returns A `pw::i2c::Address` instance.
  explicit Address(uint16_t address);

  /// Gets the 7-bit address that was provided when this instance was created.
  ///
  /// This method does a runtime assertion to ensure that the address is 7 bits
  /// or less.
  ///
  /// @returns A 7-bit address as an unsigned integer.
  uint8_t GetSevenBit() const;

  /// Gets the 10-bit address that was provided when this instance was created.
  ///
  /// @returns A 10-bit address as an unsigned integer.
  uint16_t GetTenBit() const { return address_; }

 private:
  enum AlreadyCheckedAddress { kAlreadyCheckedAddress };
  constexpr Address(uint16_t address, AlreadyCheckedAddress)
      : address_(address) {}

  uint16_t address_;
};

}  // namespace pw::i2c
