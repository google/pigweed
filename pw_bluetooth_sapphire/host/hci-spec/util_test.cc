// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"

#include "pw_unit_test/framework.h"

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
