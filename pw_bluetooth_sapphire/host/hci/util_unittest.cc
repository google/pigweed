// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include "gtest/gtest.h"

namespace btlib {
namespace hci {
namespace {

using common::DeviceAddress;
using common::DeviceAddressBytes;
using common::StaticByteBuffer;

TEST(HCI_UtilTest, DeviceAddressFromAdvReportParsesAddress) {
  StaticByteBuffer<sizeof(LEAdvertisingReportData)> buffer;
  auto* report =
      reinterpret_cast<LEAdvertisingReportData*>(buffer.mutable_data());
  report->address = common::DeviceAddressBytes({0, 1, 2, 3, 4, 5});
  report->address_type = LEAddressType::kPublicIdentity;

  DeviceAddress address;
  bool resolved;

  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, address.type());
  EXPECT_TRUE(resolved);

  report->address_type = LEAddressType::kPublic;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, address.type());
  EXPECT_FALSE(resolved);

  report->address_type = LEAddressType::kRandomIdentity;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLERandom, address.type());
  EXPECT_TRUE(resolved);

  report->address_type = LEAddressType::kRandom;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLERandom, address.type());
  EXPECT_FALSE(resolved);
}

}  // namespace
}  // namespace hci
}  // namespace btlib
