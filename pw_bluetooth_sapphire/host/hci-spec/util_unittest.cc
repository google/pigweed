// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"

#include <gtest/gtest.h>

namespace bt::hci_spec {
namespace {

// Bit values used in this test are given in a table in Core Spec Volume 4, Part
// E, Section 7.8.53.
TEST(UtilTest, AdvertisingTypeToEventBits) {
  std::optional<hci_spec::AdvertisingEventBits> bits =
      AdvertisingTypeToEventBits(pw::bluetooth::emboss::LEAdvertisingType::
                                     CONNECTABLE_AND_SCANNABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010011, bits.value());

  bits = AdvertisingTypeToEventBits(pw::bluetooth::emboss::LEAdvertisingType::
                                        CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010101, bits.value());

  bits = AdvertisingTypeToEventBits(pw::bluetooth::emboss::LEAdvertisingType::
                                        CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00011101, bits.value());

  bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::SCANNABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010010, bits.value());

  bits = AdvertisingTypeToEventBits(
      pw::bluetooth::emboss::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED);
  ASSERT_TRUE(bits);
  EXPECT_EQ(0b00010000, bits.value());
}

TEST(UtilTest, LinkKeyTypeToString) {
  std::string link_key_type =
      LinkKeyTypeToString(hci_spec::LinkKeyType::kCombination);
  EXPECT_EQ("kCombination", link_key_type);

  link_key_type = LinkKeyTypeToString(hci_spec::LinkKeyType::kLocalUnit);
  EXPECT_EQ("kLocalUnit", link_key_type);

  link_key_type = LinkKeyTypeToString(hci_spec::LinkKeyType::kRemoteUnit);
  EXPECT_EQ("kRemoteUnit", link_key_type);

  link_key_type = LinkKeyTypeToString(hci_spec::LinkKeyType::kDebugCombination);
  EXPECT_EQ("kDebugCombination", link_key_type);

  link_key_type = LinkKeyTypeToString(
      hci_spec::LinkKeyType::kUnauthenticatedCombination192);
  EXPECT_EQ("kUnauthenticatedCombination192", link_key_type);

  link_key_type =
      LinkKeyTypeToString(hci_spec::LinkKeyType::kAuthenticatedCombination192);
  EXPECT_EQ("kAuthenticatedCombination192", link_key_type);

  link_key_type =
      LinkKeyTypeToString(hci_spec::LinkKeyType::kChangedCombination);
  EXPECT_EQ("kChangedCombination", link_key_type);

  link_key_type = LinkKeyTypeToString(
      hci_spec::LinkKeyType::kUnauthenticatedCombination256);
  EXPECT_EQ("kUnauthenticatedCombination256", link_key_type);

  link_key_type =
      LinkKeyTypeToString(hci_spec::LinkKeyType::kAuthenticatedCombination256);
  EXPECT_EQ("kAuthenticatedCombination256", link_key_type);

  // Unknown link key type
  link_key_type = LinkKeyTypeToString(static_cast<hci_spec::LinkKeyType>(0xFF));
  EXPECT_EQ("(Unknown)", link_key_type);
}

}  // namespace
}  // namespace bt::hci_spec
