// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/discovery_filter.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"

namespace bt {
namespace gap {
namespace {

TEST(GAP_DiscoveryFilterTest, Flags) {
  const BufferView kEmptyData;
  const auto kInvalidFlagsData = CreateStaticByteBuffer(0x01, 0x01);
  const auto kValidFlagsData = CreateStaticByteBuffer(0x02, 0x01, 0b101);

  DiscoveryFilter filter;

  // Empty filter should match everything.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b100);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b001);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  // The following filters set multiple bits. As long as one of them is set, the
  // filter should match.
  filter.set_flags(0b101);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b111);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b011);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b010);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  // The following filters requre that *all* bits be present in the advertising
  // data.
  filter.set_flags(0b101, true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b111, true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b011, true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));

  filter.set_flags(0b010, true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, Connectable) {
  BufferView empty;
  DiscoveryFilter filter;

  // Empty filter should match both.
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, true, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, false, hci::kRSSIInvalid));

  // Filter connectable.
  filter.set_connectable(true);
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, true, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(empty, false, hci::kRSSIInvalid));

  // Filter not connectable.
  filter.set_connectable(false);
  EXPECT_FALSE(filter.MatchLowEnergyResult(empty, true, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, 16BitServiceUuids) {
  constexpr uint16_t kUuid0 = 0x180d;
  constexpr uint16_t kUuid1 = 0x1800;

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const auto kIncompleteEmpty = CreateStaticByteBuffer(0x01, 0x02);
  const auto kIncompleteNoMatch = CreateStaticByteBuffer(0x05, 0x02, 0x01, 0x02, 0x03, 0x04);
  const auto kIncompleteMatch1 = CreateStaticByteBuffer(0x05, 0x02, 0x01, 0x02, 0x0d, 0x18);
  const auto kIncompleteMatch2 = CreateStaticByteBuffer(0x05, 0x02, 0x00, 0x18, 0x03, 0x04);
  const auto kCompleteEmpty = CreateStaticByteBuffer(0x01, 0x03);
  const auto kCompleteNoMatch = CreateStaticByteBuffer(0x05, 0x03, 0x01, 0x02, 0x03, 0x04);
  const auto kCompleteMatch1 = CreateStaticByteBuffer(0x05, 0x03, 0x01, 0x02, 0x0d, 0x18);
  const auto kCompleteMatch2 = CreateStaticByteBuffer(0x05, 0x03, 0x00, 0x18, 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, 32BitServiceUuids) {
  constexpr uint16_t kUuid0 = 0x180d;
  constexpr uint16_t kUuid1 = 0x1800;

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const auto kIncompleteEmpty = CreateStaticByteBuffer(0x01, 0x04);
  const auto kIncompleteNoMatch =
      CreateStaticByteBuffer(0x09, 0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);
  const auto kIncompleteMatch1 =
      CreateStaticByteBuffer(0x09, 0x04, 0x01, 0x02, 0x03, 0x04, 0x0d, 0x18, 0x00, 0x00);
  const auto kIncompleteMatch2 =
      CreateStaticByteBuffer(0x09, 0x04, 0x00, 0x18, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04);
  const auto kCompleteEmpty = CreateStaticByteBuffer(0x01, 0x05);
  const auto kCompleteNoMatch =
      CreateStaticByteBuffer(0x09, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);
  const auto kCompleteMatch1 =
      CreateStaticByteBuffer(0x09, 0x05, 0x01, 0x02, 0x03, 0x04, 0x0d, 0x18, 0x00, 0x00);
  const auto kCompleteMatch2 =
      CreateStaticByteBuffer(0x09, 0x05, 0x00, 0x18, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, 128BitServiceUuids) {
  constexpr uint16_t kUuid0 = 0x180d;
  constexpr uint16_t kUuid1 = 0x1800;

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const auto kIncompleteEmpty = CreateStaticByteBuffer(0x01, 0x06);
  const auto kIncompleteNoMatch =
      CreateStaticByteBuffer(0x11, 0x06,

                             // UUID
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F);
  const auto kIncompleteMatch1 =
      CreateStaticByteBuffer(0x21, 0x06,

                             // UUID 1
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F,

                             // UUID 2
                             0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                             0x0d, 0x18, 0x00, 0x00);
  const auto kIncompleteMatch2 =
      CreateStaticByteBuffer(0x21, 0x06,

                             // UUID 1
                             0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                             0x00, 0x18, 0x00, 0x00,

                             // UUID 2
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F);
  const auto kCompleteEmpty = CreateStaticByteBuffer(0x01, 0x07);
  const auto kCompleteNoMatch =
      CreateStaticByteBuffer(0x11, 0x07,

                             // UUID
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F);
  const auto kCompleteMatch1 =
      CreateStaticByteBuffer(0x21, 0x07,

                             // UUID 1
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F,

                             // UUID 2
                             0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                             0x0d, 0x18, 0x00, 0x00);
  const auto kCompleteMatch2 =
      CreateStaticByteBuffer(0x21, 0x07,

                             // UUID 1
                             0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                             0x00, 0x18, 0x00, 0x00,

                             // UUID 2
                             0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                             0x0C, 0x0D, 0x0E, 0x0F);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch2, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch2, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, NameSubstring) {
  const BufferView kEmptyData;
  const auto kShortenedName = CreateStaticByteBuffer(0x05, 0x08, 'T', 'e', 's', 't');
  const auto kCompleteName = CreateStaticByteBuffer(0x0E, 0x09, 'T', 'e', 's', 't', ' ', 'C', 'o',
                                                    'm', 'p', 'l', 'e', 't', 'e');

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci::kRSSIInvalid));

  // Assigning an empty string for the name filter should have the same effect
  // as an empty filter.
  filter.set_name_substring("");
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci::kRSSIInvalid));

  filter.set_name_substring("foo");
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kShortenedName, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteName, false, hci::kRSSIInvalid));

  filter.set_name_substring("est");
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci::kRSSIInvalid));

  filter.set_name_substring("Compl");
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kShortenedName, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, RSSI) {
  constexpr int8_t kRSSIThreshold = 60;
  const BufferView kEmptyData;

  DiscoveryFilter filter;
  filter.set_rssi(hci::kRSSIInvalid);

  // |result| reports an invalid RSSI. This should fail to match even though the
  // value numerically satisfies the filter.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci::kRSSIInvalid));

  filter.set_rssi(kRSSIThreshold);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci::kRSSIInvalid));

  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kRSSIThreshold));

  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kRSSIThreshold + 1));

  // When a pathloss filter value is set and the scan result does not satisfy it
  // because it didn't include the transmission power level, the filter should
  // match since an RSSI value has been set which was used as a fallback.
  filter.set_pathloss(5);
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kRSSIThreshold + 1));

  // Finally, an empty filter should always succeed.
  filter.Reset();
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kRSSIThreshold + 1));
}

TEST(GAP_DiscoveryFilterTest, Pathloss) {
  constexpr int8_t kPathlossThreshold = 70;
  constexpr int8_t kTxPower = 5;
  constexpr int8_t kMatchingRSSI = -65;
  constexpr int8_t kNotMatchingRSSI = -66;
  constexpr int8_t kTooLargeRSSI = 71;

  const BufferView kEmptyData;
  const auto kDataWithTxPower = CreateStaticByteBuffer(0x02, 0x0A, kTxPower);

  DiscoveryFilter filter;
  filter.set_pathloss(kPathlossThreshold);

  // No Tx Power and no RSSI. Filter should not match.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci::kRSSIInvalid));

  // Tx Power is reported but RSSI is unknown. Filter should not match.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, hci::kRSSIInvalid));

  // RSSI is known but Tx Power is not reported.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, kMatchingRSSI));

  // RSSI and Tx Power are present and pathloss is within threshold.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kMatchingRSSI));

  // RSSI and Tx Power are present but RSSI is larger than Tx Power.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kTooLargeRSSI));

  // RSSI and Tx Power are present but pathloss is above threshold.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kNotMatchingRSSI));

  // Assign a RSSI filter. Even though this field alone WOULD satisfy the
  // filter, the match function should not fall back to it when Tx Power is
  // present and the pathloss filter is unsatisfied.
  filter.set_rssi(kNotMatchingRSSI);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kNotMatchingRSSI));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kNotMatchingRSSI));

  // Finally, an empty filter should always succeed.
  filter.Reset();
  EXPECT_TRUE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kNotMatchingRSSI));
}

TEST(GAP_DiscoveryFilterTest, ManufacturerCode) {
  const BufferView kEmptyData;
  const auto kValidData0 = CreateStaticByteBuffer(0x03, 0xFF, 0xE0, 0x00);
  const auto kValidData1 = CreateStaticByteBuffer(0x06, 0xFF, 0xE0, 0x00, 0x01, 0x02, 0x03);
  const auto kInvalidData0 = CreateStaticByteBuffer(0x02, 0xFF, 0xE0);
  const auto kInvalidData1 = CreateStaticByteBuffer(0x03, 0xFF, 0x4C, 0x00);

  DiscoveryFilter filter;

  // Empty filter should match everything.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData0, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData1, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidData0, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidData1, false, hci::kRSSIInvalid));

  filter.set_manufacturer_code(0x00E0);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData0, false, hci::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData1, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidData0, false, hci::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidData1, false, hci::kRSSIInvalid));
}

TEST(GAP_DiscoveryFilterTest, Combined) {
  constexpr int8_t kMatchingPathlossThreshold = 70;
  constexpr int8_t kNotMatchingPathlossThreshold = 69;
  constexpr int8_t kTxPower = 5;
  constexpr int8_t kRSSI = -65;

  constexpr uint16_t kMatchingUuid = 0x180d;
  constexpr uint16_t kNotMatchingUuid = 0x1800;

  constexpr char kMatchingName[] = "test";
  constexpr char kNotMatchingName[] = "foo";

  const auto kAdvertisingData = CreateStaticByteBuffer(
      // Flags
      0x02, 0x01, 0x01,

      // 16 Bit UUIDs
      0x03, 0x02, 0x0d, 0x18,

      // Complete name
      0x05, 0x09, 't', 'e', 's', 't',

      // Tx Power Level
      0x02, 0x0A, kTxPower,

      // Manufacturer specific data
      0x05, 0xFF, 0xE0, 0x00, 0x01, 0x02);

  DiscoveryFilter filter;

  // Empty filter should match.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));

  // Assign all fields and make them match.
  filter.set_flags(0x01);
  filter.set_connectable(true);
  filter.set_service_uuids(std::vector<UUID>{UUID(kMatchingUuid)});
  filter.set_name_substring(kMatchingName);
  filter.set_pathloss(kMatchingPathlossThreshold);
  filter.set_manufacturer_code(0x00E0);
  EXPECT_TRUE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));

  // Toggle each field one by one to test that a single mismatch causes the
  // filter to fail.
  filter.set_flags(0x03, true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_flags(0x01);

  filter.set_connectable(false);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_connectable(true);

  filter.set_service_uuids(std::vector<UUID>{UUID(kNotMatchingUuid)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_service_uuids(std::vector<UUID>{UUID(kMatchingUuid)});

  filter.set_name_substring(kNotMatchingName);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_name_substring(kMatchingName);

  filter.set_pathloss(kNotMatchingPathlossThreshold);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_pathloss(kMatchingPathlossThreshold);

  filter.set_manufacturer_code(0x004C);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_manufacturer_code(0x00E0);

  EXPECT_TRUE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
}

TEST(GAP_DiscoveryFilterTest, GeneralDiscoveryFlags) {
  const auto kLimitedDiscoverableData = CreateStaticByteBuffer(
      // Flags
      0x02, 0x01, 0x01);
  const auto kGeneralDiscoverableData = CreateStaticByteBuffer(
      // Flags
      0x02, 0x01, 0x02);
  const auto kNonDiscoverableData = CreateStaticByteBuffer(
      // Flags (all flags are set except for discoverability).
      0x02, 0x01, 0xFC);

  DiscoveryFilter filter;
  filter.SetGeneralDiscoveryFlags();

  EXPECT_TRUE(filter.MatchLowEnergyResult(kLimitedDiscoverableData, true, 0));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kGeneralDiscoverableData, true, 0));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kNonDiscoverableData, true, 0));
}

}  // namespace
}  // namespace gap
}  // namespace bt
