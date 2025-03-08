// Copyright 2021 The Pigweed Authors
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

#include "pw_i2c/address.h"

#include "pw_unit_test/framework.h"

namespace pw::i2c {
namespace {

TEST(Address, SevenBitConstexpr) {
  constexpr Address kSevenBit =
      Address::SevenBit<Address::kMaxSevenBitAddress>();
  EXPECT_EQ(kSevenBit.GetSevenBit(), Address::kMaxSevenBitAddress);
}

TEST(Address, TenBitConstexpr) {
  constexpr Address kTenBit = Address::TenBit<Address::kMaxTenBitAddress>();
  EXPECT_EQ(kTenBit.GetTenBit(), Address::kMaxTenBitAddress);
}

TEST(Address, SevenBitRuntimeChecked) {
  const Address seven_bit(Address::kMaxSevenBitAddress);
  EXPECT_EQ(seven_bit.GetSevenBit(), Address::kMaxSevenBitAddress);
}

TEST(Address, TenBitRuntimeChecked) {
  const Address ten_bit(Address::kMaxTenBitAddress);
  EXPECT_EQ(ten_bit.GetTenBit(), Address::kMaxTenBitAddress);
}

TEST(Address, Equality) {
  EXPECT_TRUE(Address::SevenBit<0x3A>() == Address::SevenBit<0x3A>());
  EXPECT_FALSE(Address::SevenBit<0x3A>() == Address::SevenBit<0x3F>());
  EXPECT_FALSE(Address::SevenBit<0x3F>() == Address::TenBit<0x3F>());
  EXPECT_TRUE(Address::TenBit<0x3F>() == Address::TenBit<0x3F>());
}

TEST(Address, GetAddressSeven) {
  constexpr uint16_t kAddressValue = 0x3A;
  constexpr Address test_addr = Address::SevenBit<kAddressValue>();
  EXPECT_EQ(test_addr.GetAddress(), kAddressValue);
}

TEST(Address, GetAddressTen) {
  constexpr uint16_t kAddressValue = 0xAA;
  constexpr Address test_addr = Address::TenBit<kAddressValue>();
  EXPECT_EQ(test_addr.GetAddress(), kAddressValue);
}

TEST(Address, IsTenBitTrue) {
  // Full ten bit address.
  EXPECT_TRUE(Address::TenBit<0xAA>().IsTenBit());

  // Ten bit address but value that only uses 7 bits.
  EXPECT_TRUE(Address::TenBit<0x3A>().IsTenBit());

  // Seven bit address.
  EXPECT_FALSE(Address::SevenBit<0x3A>().IsTenBit());
}

// TODO: b/235289499 - Verify assert behaviour when trying to get a 7bit address
// out of a 10bit address.

// TODO: b/234882063 - Add tests to ensure the constexpr constructors fail to
// compile with invalid addresses once no-copmile tests are set up in Pigweed.

}  // namespace
}  // namespace pw::i2c
