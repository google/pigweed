// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include <gtest/gtest.h>

namespace bt::hci_spec {
namespace {

// Bit values used in this test are given in a table in Core Spec Volume 4, Part E, Section 7.8.53.
TEST(UtilTest, AdvertisingTypeToEventBits) {
  std::optional<hci_spec::AdvertisingEventBits> bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::CONNECTABLE_AND_SCANNABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010011, bits.value());

  bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010101, bits.value());

  bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00011101, bits.value());

  bits = AdvertisingTypeToEventBits(pw::bluetooth::emboss::LEAdvertisingType::SCANNABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010010, bits.value());

  bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010000, bits.value());
}

}  // namespace
}  // namespace bt::hci_spec
