// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_packet_filter.h"

#include "gtest/gtest.h"
#include "pw_bluetooth_sapphire/internal/host/common/advertising_data.h"
#include "pw_bluetooth_sapphire/internal/host/hci/discovery_filter.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"

namespace bt::hci {

using bt::testing::FakeController;
using TestingBase = bt::testing::FakeDispatcherControllerTest<FakeController>;

class AdvertisingPacketFilterTest : public TestingBase {
 public:
  AdvertisingPacketFilterTest() = default;
  ~AdvertisingPacketFilterTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    this->test_device()->set_settings(settings);
  }

  void TearDown() override {
    this->test_device()->Stop();
    TestingBase::TearDown();
  }

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingPacketFilterTest);
};

TEST_F(AdvertisingPacketFilterTest, SetUnsetPacketFilters) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());
  ASSERT_EQ(0u, packet_filter.scan_ids().size());

  packet_filter.SetPacketFilters(0, {});
  ASSERT_EQ(1u, packet_filter.scan_ids().size());
  EXPECT_EQ(1u, packet_filter.scan_ids().count(0));

  packet_filter.UnsetPacketFilters(0);
  ASSERT_EQ(0u, packet_filter.scan_ids().size());
  EXPECT_EQ(0u, packet_filter.scan_ids().count(0));
}

TEST_F(AdvertisingPacketFilterTest, FilterWithNoScanId) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());
  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), true, 0));
}

TEST_F(AdvertisingPacketFilterTest, FilterWithEmptyFilters) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());
  packet_filter.SetPacketFilters(0, {});
  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), true, 0));
}

TEST_F(AdvertisingPacketFilterTest, Filter) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());

  DiscoveryFilter filter;
  filter.set_connectable(true);
  packet_filter.SetPacketFilters(0, {filter});

  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), true, 0));
  EXPECT_FALSE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), false, 0));
}

TEST_F(AdvertisingPacketFilterTest, MultipleScanIds) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());

  DiscoveryFilter filter_a;
  filter_a.set_connectable(true);
  packet_filter.SetPacketFilters(0, {filter_a});

  DiscoveryFilter filter_b;
  filter_b.set_name_substring("bluetooth");
  packet_filter.SetPacketFilters(1, {filter_b});

  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), true, 0));
  EXPECT_FALSE(packet_filter.Matches(
      1, fit::error(AdvertisingData::ParseError::kMissing), true, 0));

  {
    AdvertisingData ad;
    ASSERT_TRUE(ad.SetLocalName("a bluetooth device"));
    EXPECT_FALSE(packet_filter.Matches(0, fit::ok(std::move(ad)), false, 0));
  }

  {
    AdvertisingData ad;
    ASSERT_TRUE(ad.SetLocalName("a bluetooth device"));
    EXPECT_TRUE(packet_filter.Matches(1, fit::ok(std::move(ad)), false, 0));
  }
}

TEST_F(AdvertisingPacketFilterTest, SetPacketFiltersReplacesPrevious) {
  AdvertisingPacketFilter packet_filter({false, 0}, transport()->GetWeakPtr());

  packet_filter.SetPacketFilters(0, {});
  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), false, 0));

  DiscoveryFilter filter;
  filter.set_connectable(true);
  packet_filter.SetPacketFilters(0, {filter});

  EXPECT_FALSE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), false, 0));
  EXPECT_TRUE(packet_filter.Matches(
      0, fit::error(AdvertisingData::ParseError::kMissing), true, 0));
}

}  // namespace bt::hci
