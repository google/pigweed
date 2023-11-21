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
