// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/util.h"

#include <endian.h>
#include <gtest/gtest.h>

namespace bt::hci {
namespace {

TEST(UtilTest, DeviceAddressFromAdvReportParsesAddress) {
  StaticByteBuffer<sizeof(hci_spec::LEAdvertisingReportData)> buffer;
  auto* report = reinterpret_cast<hci_spec::LEAdvertisingReportData*>(
      buffer.mutable_data());
  report->address = DeviceAddressBytes({0, 1, 2, 3, 4, 5});
  report->address_type = hci_spec::LEAddressType::kPublicIdentity;

  DeviceAddress address;
  bool resolved;

  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, address.type());
  EXPECT_TRUE(resolved);

  report->address_type = hci_spec::LEAddressType::kPublic;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, address.type());
  EXPECT_FALSE(resolved);

  report->address_type = hci_spec::LEAddressType::kRandomIdentity;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLERandom, address.type());
  EXPECT_TRUE(resolved);

  report->address_type = hci_spec::LEAddressType::kRandom;
  EXPECT_TRUE(DeviceAddressFromAdvReport(*report, &address, &resolved));
  EXPECT_EQ(DeviceAddress::Type::kLERandom, address.type());
  EXPECT_FALSE(resolved);
}

}  // namespace
}  // namespace bt::hci
