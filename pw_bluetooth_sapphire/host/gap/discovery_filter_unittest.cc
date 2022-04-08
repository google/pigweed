// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/discovery_filter.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/supplement_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"

namespace bt::gap {
namespace {

constexpr uint16_t kUuid0 = 0x180d;

TEST(DiscoveryFilterTest, Flags) {
  const BufferView kEmptyData;
  const StaticByteBuffer kInvalidFlagsData(0x01, 0x01);
  const StaticByteBuffer kValidFlagsData(0x02, 0x01, 0b101);

  DiscoveryFilter filter;

  // Empty filter should match everything.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b100);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b001);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  // The following filters set multiple bits. As long as one of them is set, the
  // filter should match.
  filter.set_flags(0b101);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b111);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b011);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b010);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  // The following filters requre that *all* bits be present in the advertising
  // data.
  filter.set_flags(0b101, /*require_all=*/true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b111, /*require_all=*/true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b011, /*require_all=*/true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));

  filter.set_flags(0b010, /*require_all=*/true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidFlagsData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kValidFlagsData, false, hci_spec::kRSSIInvalid));
}

TEST(DiscoveryFilterTest, Connectable) {
  BufferView empty;
  DiscoveryFilter filter;

  // Empty filter should match both.
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, true, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, false, hci_spec::kRSSIInvalid));

  // Filter connectable.
  filter.set_connectable(true);
  EXPECT_TRUE(filter.connectable());
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, true, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(empty, false, hci_spec::kRSSIInvalid));

  // Filter not connectable.
  filter.set_connectable(false);
  EXPECT_TRUE(filter.connectable());
  EXPECT_FALSE(filter.MatchLowEnergyResult(empty, true, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(empty, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_FALSE(filter.connectable());
}

TEST(DiscoveryFilterTest, 16BitServiceUuids) {
  constexpr uint16_t kUuid1 = 0x1800;

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const StaticByteBuffer kIncompleteEmpty(0x01, DataType::kIncomplete16BitServiceUuids);
  const StaticByteBuffer kIncompleteNoMatch(0x05, DataType::kIncomplete16BitServiceUuids, 0x01,
                                            0x02, 0x03, 0x04);
  const auto kIncompleteMatch0 = StaticByteBuffer(0x05, DataType::kIncomplete16BitServiceUuids,
                                                  0x01, 0x02, LowerBits(kUuid0), UpperBits(kUuid0));
  const auto kIncompleteMatch1 = StaticByteBuffer(0x05, DataType::kIncomplete16BitServiceUuids,
                                                  LowerBits(kUuid1), UpperBits(kUuid1), 0x03, 0x04);
  const auto kCompleteEmpty = StaticByteBuffer(0x01, DataType::kComplete16BitServiceUuids);
  const StaticByteBuffer kCompleteNoMatch(0x05, DataType::kComplete16BitServiceUuids, 0x01, 0x02,
                                          0x03, 0x04);
  const auto kCompleteMatch0 = StaticByteBuffer(0x05, DataType::kComplete16BitServiceUuids, 0x01,
                                                0x02, LowerBits(kUuid0), UpperBits(kUuid0));
  const auto kCompleteMatch1 = StaticByteBuffer(0x05, DataType::kComplete16BitServiceUuids,
                                                LowerBits(kUuid1), UpperBits(kUuid1), 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_uuids().empty());
}

TEST(DiscoveryFilterTest, 32BitServiceUuids) {
  constexpr uint32_t kUuid1 = 0xabcd1800;

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const auto kIncompleteEmpty = StaticByteBuffer(0x01, 0x04);
  const auto kIncompleteNoMatch = StaticByteBuffer(0x09, DataType::kIncomplete32BitServiceUuids,
                                                   // First UUID
                                                   0x01, 0x02, 0x03, 0x04,
                                                   // Second UUID
                                                   0x05, 0x06, 0x07, 0x08);
  const auto kIncompleteMatch0 = StaticByteBuffer(0x09, DataType::kIncomplete32BitServiceUuids,
                                                  // First UUID
                                                  0x01, 0x02, 0x03, 0x04,
                                                  // kUuid0
                                                  LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kIncompleteMatch1 = StaticByteBuffer(0x09, DataType::kIncomplete32BitServiceUuids,
                                                  // kUuid1
                                                  0x00, 0x18, 0xcd, 0xab,
                                                  // Second UUID
                                                  0x01, 0x02, 0x03, 0x04);
  const auto kCompleteEmpty = StaticByteBuffer(0x01, DataType::kComplete32BitServiceUuids);
  const auto kCompleteNoMatch = StaticByteBuffer(0x09, DataType::kComplete32BitServiceUuids,
                                                 // First UUID
                                                 0x01, 0x02, 0x03, 0x04,
                                                 // Second UUID
                                                 0x05, 0x06, 0x07, 0x08);
  const auto kCompleteMatch0 = StaticByteBuffer(0x09, DataType::kComplete32BitServiceUuids,
                                                // First UUID
                                                0x01, 0x02, 0x03, 0x04,
                                                // kUuid0
                                                LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kCompleteMatch1 = StaticByteBuffer(0x09, DataType::kComplete32BitServiceUuids,
                                                // kUuid1
                                                0x00, 0x18, 0xcd, 0xab,
                                                // Second UUID
                                                0x01, 0x02, 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_uuids().empty());
}

TEST(DiscoveryFilterTest, 128BitServiceUuids) {
  constexpr UInt128 kUuid1 = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                              0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x18, 0xcd, 0xab};

  const BufferView kEmptyData;

  // Below, "Incomplete" refers to the "Incomplete Service UUIDs" field while
  // "Complete" refers to "Complete Service UUIDs".

  const auto kIncompleteEmpty = StaticByteBuffer(0x01, DataType::kIncomplete128BitServiceUuids);
  const auto kIncompleteNoMatch = StaticByteBuffer(0x11, DataType::kIncomplete128BitServiceUuids,

                                                   // UUID
                                                   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                                   0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);
  const auto kIncompleteMatch0 =
      StaticByteBuffer(0x21, DataType::kIncomplete128BitServiceUuids,

                       // First UUID
                       0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                       0x0D, 0x0E, 0x0F,

                       // kUuid0 - padded with the BT SIG Base UUID.
                       // See Core Spec v5.0, Vol 3, Part B, Section 2.5.1.
                       0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                       LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kIncompleteMatch1 = StaticByteBuffer(
      0x21, DataType::kIncomplete128BitServiceUuids,

      // kUuid1
      kUuid1[0], kUuid1[1], kUuid1[2], kUuid1[3], kUuid1[4], kUuid1[5], kUuid1[6], kUuid1[7],
      kUuid1[8], kUuid1[9], kUuid1[10], kUuid1[11], kUuid1[12], kUuid1[13], kUuid1[14], kUuid1[15],

      // Second UUID
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
      0x0F);
  const auto kCompleteEmpty = StaticByteBuffer(0x01, DataType::kComplete128BitServiceUuids);
  const auto kCompleteNoMatch = StaticByteBuffer(0x11, DataType::kComplete128BitServiceUuids,

                                                 // UUID
                                                 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);
  const auto kCompleteMatch0 =
      StaticByteBuffer(0x21, DataType::kComplete128BitServiceUuids,

                       // First UUID
                       0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                       0x0D, 0x0E, 0x0F,

                       // kUuid0 - padded with the BT SIG Base UUID.
                       // See Core Spec v5.0, Vol 3, Part B, Section 2.5.1.
                       0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                       LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kCompleteMatch1 = StaticByteBuffer(
      0x21, DataType::kComplete128BitServiceUuids,

      // kUuid1
      kUuid1[0], kUuid1[1], kUuid1[2], kUuid1[3], kUuid1[4], kUuid1[5], kUuid1[6], kUuid1[7],
      kUuid1[8], kUuid1[9], kUuid1[10], kUuid1[11], kUuid1[12], kUuid1[13], kUuid1[14], kUuid1[15],

      // Second UUID
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
      0x0F);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kIncompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kIncompleteMatch1, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_uuids().empty());
}

TEST(DiscoveryFilterTest, 16BitServiceDataUuids) {
  constexpr uint16_t kUuid1 = 0x1800;

  const BufferView kEmptyData;

  const auto kEmpty = StaticByteBuffer(0x01, DataType::kServiceData16Bit);
  const auto kNoMatch = StaticByteBuffer(0x05, DataType::kServiceData16Bit, 0x01, 0x02, 0x03, 0x04);
  const auto kMatch0 = StaticByteBuffer(0x05, DataType::kServiceData16Bit, 0x01, 0x02,
                                        LowerBits(kUuid0), UpperBits(kUuid0));
  const auto kMatch1 = StaticByteBuffer(0x05, DataType::kServiceData16Bit, LowerBits(kUuid1),
                                        UpperBits(kUuid1), 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_data_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_data_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_data_uuids().empty());
}

TEST(DiscoveryFilterTest, 32BitServiceDataUuids) {
  constexpr uint32_t kUuid1 = 0xabcd1800;

  const BufferView kEmptyData;

  const auto kEmpty = StaticByteBuffer(0x01, DataType::kServiceData32Bit);
  const auto kNoMatch = StaticByteBuffer(0x09, DataType::kServiceData32Bit,
                                         // Random UUID
                                         0x01, 0x02, 0x03, 0x04,
                                         // Random UUID
                                         0x05, 0x06, 0x07, 0x08);
  const auto kMatch0 = StaticByteBuffer(0x09, DataType::kServiceData32Bit,
                                        // Random UUID
                                        0x01, 0x02, 0x03, 0x04,
                                        // kUuid0
                                        LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kMatch1 = StaticByteBuffer(0x09, DataType::kServiceData32Bit,
                                        // kUuid1
                                        0x00, 0x18, 0xcd, 0xab,
                                        // Random UUID
                                        0x01, 0x02, 0x03, 0x04);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_data_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_data_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_data_uuids().empty());
}

TEST(DiscoveryFilterTest, 128BitServiceDataUuids) {
  constexpr UInt128 kUuid1 = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
                              0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x18, 0xcd, 0xab};

  const BufferView kEmptyData;

  const auto kEmpty = StaticByteBuffer(0x01, DataType::kServiceData128Bit);
  const auto kNoMatch = StaticByteBuffer(0x11, DataType::kServiceData128Bit,

                                         // Random UUID
                                         0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                         0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);
  const auto kMatch0 =
      StaticByteBuffer(0x21, DataType::kServiceData128Bit,

                       // Random UUID
                       0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                       0x0D, 0x0E, 0x0F,

                       // kUuid0 - padded with the BT SIG Base UUID. See Core Spec v5.0, Vol 3,
                       // Part B, Section 2.5.1.
                       0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
                       LowerBits(kUuid0), UpperBits(kUuid0), 0x00, 0x00);
  const auto kMatch1 = StaticByteBuffer(
      0x21, DataType::kServiceData128Bit,

      // kUuid1
      kUuid1[0], kUuid1[1], kUuid1[2], kUuid1[3], kUuid1[4], kUuid1[5], kUuid1[6], kUuid1[7],
      kUuid1[8], kUuid1[9], kUuid1[10], kUuid1[11], kUuid1[12], kUuid1[13], kUuid1[14], kUuid1[15],

      // Random UUID
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
      0x0F);

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  // Filter for kUuid0 and kUuid1.
  filter.set_service_data_uuids(std::vector<UUID>{UUID(kUuid0), UUID(kUuid1)});
  EXPECT_FALSE(filter.service_data_uuids().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmpty, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kNoMatch, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kMatch1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.service_data_uuids().empty());
}

TEST(DiscoveryFilterTest, NameSubstring) {
  const BufferView kEmptyData;
  const auto kShortenedName = StaticByteBuffer(0x05, 0x08, 'T', 'e', 's', 't');
  const auto kCompleteName =
      StaticByteBuffer(0x0E, 0x09, 'T', 'e', 's', 't', ' ', 'C', 'o', 'm', 'p', 'l', 'e', 't', 'e');

  DiscoveryFilter filter;

  // An empty filter should match all payloads.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci_spec::kRSSIInvalid));

  // Assigning an empty string for the name filter should have the same effect
  // as an empty filter.
  filter.set_name_substring("");
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci_spec::kRSSIInvalid));

  filter.set_name_substring("foo");
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kShortenedName, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kCompleteName, false, hci_spec::kRSSIInvalid));

  filter.set_name_substring("est");
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kShortenedName, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci_spec::kRSSIInvalid));

  filter.set_name_substring("Compl");
  EXPECT_FALSE(filter.name_substring().empty());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kShortenedName, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kCompleteName, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_TRUE(filter.name_substring().empty());
}

TEST(DiscoveryFilterTest, RSSI) {
  constexpr int8_t kRSSIThreshold = 60;
  const BufferView kEmptyData;

  DiscoveryFilter filter;
  filter.set_rssi(hci_spec::kRSSIInvalid);

  // |result| reports an invalid RSSI. This should fail to match even though the
  // value numerically satisfies the filter.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci_spec::kRSSIInvalid));

  filter.set_rssi(kRSSIThreshold);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci_spec::kRSSIInvalid));

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

TEST(DiscoveryFilterTest, Pathloss) {
  constexpr int8_t kPathlossThreshold = 70;
  constexpr int8_t kTxPower = 5;
  constexpr int8_t kMatchingRSSI = -65;
  constexpr int8_t kNotMatchingRSSI = -66;
  constexpr int8_t kTooLargeRSSI = 71;

  const BufferView kEmptyData;
  const auto kDataWithTxPower = StaticByteBuffer(0x02, 0x0A, kTxPower);

  DiscoveryFilter filter;
  filter.set_pathloss(kPathlossThreshold);

  // No Tx Power and no RSSI. Filter should not match.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, true, hci_spec::kRSSIInvalid));

  // Tx Power is reported but RSSI is unknown. Filter should not match.
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, hci_spec::kRSSIInvalid));

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
  EXPECT_TRUE(filter.pathloss());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kNotMatchingRSSI));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, true, kNotMatchingRSSI));

  // Finally, an empty filter should always succeed.
  filter.Reset();
  EXPECT_FALSE(filter.pathloss());
  EXPECT_TRUE(filter.MatchLowEnergyResult(kDataWithTxPower, true, kNotMatchingRSSI));
}

TEST(DiscoveryFilterTest, ManufacturerCode) {
  const BufferView kEmptyData;
  const auto kValidData0 = StaticByteBuffer(0x03, 0xFF, 0xE0, 0x00);
  const auto kValidData1 = StaticByteBuffer(0x06, 0xFF, 0xE0, 0x00, 0x01, 0x02, 0x03);
  const auto kInvalidData0 = StaticByteBuffer(0x02, 0xFF, 0xE0);
  const auto kInvalidData1 = StaticByteBuffer(0x03, 0xFF, 0x4C, 0x00);

  DiscoveryFilter filter;

  // Empty filter should match everything.
  EXPECT_TRUE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData1, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidData0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kInvalidData1, false, hci_spec::kRSSIInvalid));

  filter.set_manufacturer_code(0x00E0);
  EXPECT_TRUE(filter.manufacturer_code());
  EXPECT_FALSE(filter.MatchLowEnergyResult(kEmptyData, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData0, false, hci_spec::kRSSIInvalid));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kValidData1, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidData0, false, hci_spec::kRSSIInvalid));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kInvalidData1, false, hci_spec::kRSSIInvalid));

  filter.Reset();
  EXPECT_FALSE(filter.manufacturer_code());
}

TEST(DiscoveryFilterTest, Combined) {
  constexpr int8_t kMatchingPathlossThreshold = 70;
  constexpr int8_t kNotMatchingPathlossThreshold = 69;
  constexpr int8_t kTxPower = 5;
  constexpr int8_t kRSSI = -65;

  constexpr uint16_t kMatchingUuid = 0x180d;
  constexpr uint16_t kNotMatchingUuid = 0x1800;
  constexpr uint16_t kMatchingServiceDataUuid = 0x1234;
  constexpr uint16_t kNotMatchingServiceDataUuid = 0x5678;

  constexpr char kMatchingName[] = "test";
  constexpr char kNotMatchingName[] = "foo";

  const auto kAdvertisingData = StaticByteBuffer(
      // Flags
      0x02, 0x01, 0x01,

      // 16 Bit Service UUIDs
      0x03, 0x02, 0x0d, 0x18,

      // 16 Bit Service Data UUIDs
      0x03, DataType::kServiceData16Bit, 0x34, 0x12,

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
  filter.set_service_data_uuids(std::vector<UUID>{UUID(kMatchingServiceDataUuid)});
  filter.set_name_substring(kMatchingName);
  filter.set_pathloss(kMatchingPathlossThreshold);
  filter.set_manufacturer_code(0x00E0);
  EXPECT_TRUE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));

  // Toggle each field one by one to test that a single mismatch causes the
  // filter to fail.
  filter.set_flags(0x03, /*require_all=*/true);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_flags(0x01);

  filter.set_connectable(false);
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_connectable(true);

  filter.set_service_uuids(std::vector<UUID>{UUID(kNotMatchingUuid)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_service_uuids(std::vector<UUID>{UUID(kMatchingUuid)});

  filter.set_service_data_uuids(std::vector<UUID>{UUID(kNotMatchingServiceDataUuid)});
  EXPECT_FALSE(filter.MatchLowEnergyResult(kAdvertisingData, true, kRSSI));
  filter.set_service_data_uuids(std::vector<UUID>{UUID(kMatchingServiceDataUuid)});

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

TEST(DiscoveryFilterTest, GeneralDiscoveryFlags) {
  const auto kLimitedDiscoverableData = StaticByteBuffer(
      // Flags
      0x02, 0x01, 0x01);
  const auto kGeneralDiscoverableData = StaticByteBuffer(
      // Flags
      0x02, 0x01, 0x02);
  const auto kNonDiscoverableData = StaticByteBuffer(
      // Flags (all flags are set except for discoverability).
      0x02, 0x01, 0xFC);

  DiscoveryFilter filter;
  filter.SetGeneralDiscoveryFlags();

  EXPECT_TRUE(filter.MatchLowEnergyResult(kLimitedDiscoverableData, true, 0));
  EXPECT_TRUE(filter.MatchLowEnergyResult(kGeneralDiscoverableData, true, 0));
  EXPECT_FALSE(filter.MatchLowEnergyResult(kNonDiscoverableData, true, 0));
}

}  // namespace
}  // namespace bt::gap
