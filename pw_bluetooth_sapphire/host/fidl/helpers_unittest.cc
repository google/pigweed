// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace fble = fuchsia::bluetooth::le;
namespace fbt = fuchsia::bluetooth;

namespace bthost {
namespace fidl_helpers {
namespace {

TEST(FidlHelpersTest, AddressBytesFrommString) {
  EXPECT_FALSE(AddressBytesFromString(""));
  EXPECT_FALSE(AddressBytesFromString("FF"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:F"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FF "));
  EXPECT_FALSE(AddressBytesFromString(" FF:FF:FF:FF:FF:FF"));

  auto addr1 = AddressBytesFromString("FF:FF:FF:FF:FF:FF");
  EXPECT_TRUE(addr1);
  EXPECT_EQ("FF:FF:FF:FF:FF:FF", addr1->ToString());

  auto addr2 = AddressBytesFromString("03:7F:FF:02:0F:01");
  EXPECT_TRUE(addr2);
  EXPECT_EQ("03:7F:FF:02:0F:01", addr2->ToString());
}

TEST(FidlHelpersTest, AdvertisingIntervalFromFidl) {
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST1,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::VERY_FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::FAST2,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::FAST));
  EXPECT_EQ(bt::gap::AdvertisingInterval::SLOW,
            AdvertisingIntervalFromFidl(fble::AdvertisingModeHint::SLOW));
}

TEST(FidlHelpersTest, UuidFromFidl) {
  fbt::Uuid input;
  input.value = {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x0d,
                  0x18, 0x00, 0x00}};

  // We expect the input bytes to be carried over directly.
  bt::UUID output = UuidFromFidl(input);
  EXPECT_EQ("0000180d-0000-1000-8000-00805f9b34fb", output.ToString());
  EXPECT_EQ(2u, output.CompactSize());
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlEmpty) {
  fble::AdvertisingData input;
  ASSERT_TRUE(input.IsEmpty());

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.service_uuids().empty());
  EXPECT_TRUE(output.service_data_uuids().empty());
  EXPECT_TRUE(output.manufacturer_data_ids().empty());
  EXPECT_TRUE(output.uris().empty());
  EXPECT_FALSE(output.appearance());
  EXPECT_FALSE(output.tx_power());
  EXPECT_FALSE(output.local_name());
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlName) {
  constexpr char kTestName[] = "💩";
  fble::AdvertisingData input;
  input.set_name(kTestName);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.local_name());
  EXPECT_EQ(kTestName, *output.local_name());
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlAppearance) {
  fble::AdvertisingData input;
  input.set_appearance(fuchsia::bluetooth::Appearance::HID_DIGITIZER_TABLET);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.appearance());

  // Value comes from the standard Bluetooth "assigned numbers" document.
  EXPECT_EQ(0x03C5, *output.appearance());
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlTxPower) {
  constexpr int8_t kTxPower = -50;
  fble::AdvertisingData input;
  input.set_tx_power_level(kTxPower);

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_TRUE(output.tx_power());
  EXPECT_EQ(kTxPower, *output.tx_power());
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlUuids) {
  // The first two entries are duplicated. The resulting structure should contain no duplicates.
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid3{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  fble::AdvertisingData input;
  input.set_service_uuids({{kUuid1, kUuid2, kUuid3}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.service_uuids().size());
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid1.value)));
  EXPECT_EQ(1u, output.service_uuids().count(bt::UUID(kUuid2.value)));
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlServiceData) {
  const fbt::Uuid kUuid1{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
  const fbt::Uuid kUuid2{{16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_service_data({{{kUuid1, kData1}, {kUuid2, kData2}}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.service_data_uuids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.service_data(bt::UUID(kUuid1.value))));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.service_data(bt::UUID(kUuid2.value))));
}

TEST(FidlHelpersTest, AdvertisingDataFromFidlManufacturerData) {
  constexpr uint16_t kCompanyId1 = 1;
  constexpr uint16_t kCompanyId2 = 2;
  const std::vector<uint8_t> kData1{{'h', 'e', 'l', 'l', 'o'}};
  const std::vector<uint8_t> kData2{{'b', 'y', 'e'}};

  fble::AdvertisingData input;
  input.set_manufacturer_data({{{kCompanyId1, kData1}, {kCompanyId2, kData2}}});

  auto output = AdvertisingDataFromFidl(input);
  EXPECT_EQ(2u, output.manufacturer_data_ids().size());
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData1), output.manufacturer_data(kCompanyId1)));
  EXPECT_TRUE(ContainersEqual(bt::BufferView(kData2), output.manufacturer_data(kCompanyId2)));
}

}  // namespace
}  // namespace fidl_helpers
}  // namespace bthost
