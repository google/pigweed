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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_handle_map.h"

#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_unit_test/framework.h"

namespace bt::hci {
namespace {

using AddressTuple = std::tuple<DeviceAddress, bool>;

TEST(AdvertisingHandleMapTest, LegacyAndExtended) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});

  std::optional<hci_spec::AdvertisingHandle> legacy_handle =
      handle_map.MapHandle(address, /*extended_pdu=*/false);
  std::optional<hci_spec::AdvertisingHandle> extended_handle =
      handle_map.MapHandle(address, /*extended_pdu=*/true);

  EXPECT_TRUE(legacy_handle);
  EXPECT_TRUE(extended_handle);
  EXPECT_NE(legacy_handle, extended_handle);
}

class AdvertisingHandleMapTest : public testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(AdvertisingHandleMapTest,
                         AdvertisingHandleMapTest,
                         ::testing::Bool());

TEST_P(AdvertisingHandleMapTest, Bidirectional) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address_a = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle_a =
      handle_map.MapHandle(address_a, GetParam());
  EXPECT_LE(handle_a.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_TRUE(handle_a);

  DeviceAddress address_b = DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  std::optional<hci_spec::AdvertisingHandle> handle_b =
      handle_map.MapHandle(address_b, GetParam());
  EXPECT_TRUE(handle_b);
  EXPECT_LE(handle_b.value(), hci_spec::kMaxAdvertisingHandle);

  AddressTuple tuple_a = std::make_tuple(address_a, GetParam());
  EXPECT_EQ(tuple_a, handle_map.GetAddress(handle_a.value()));
  AddressTuple tuple_b = std::make_tuple(address_b, GetParam());
  EXPECT_EQ(tuple_b, handle_map.GetAddress(handle_b.value()));
}

TEST_P(AdvertisingHandleMapTest, MapHandleAlreadyExists) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle_0 =
      handle_map.MapHandle(address, GetParam());
  EXPECT_LE(handle_0.value(), hci_spec::kMaxAdvertisingHandle);
  ASSERT_TRUE(handle_0);

  std::optional<hci_spec::AdvertisingHandle> handle_1 =
      handle_map.MapHandle(address, GetParam());
  ASSERT_TRUE(handle_1);
  EXPECT_LE(handle_1.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_NE(handle_0.value(), handle_1.value());
}

TEST_P(AdvertisingHandleMapTest, MapHandleMoreThanSupported) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<hci_spec::AdvertisingHandle> handle =
        handle_map.MapHandle(address, GetParam());
    EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});

  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address, GetParam());
  EXPECT_FALSE(handle);
  EXPECT_EQ(handle_map.capacity(), handle_map.Size());
}

TEST_P(AdvertisingHandleMapTest, MapHandleSupportHandleReallocation) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<hci_spec::AdvertisingHandle> handle =
        handle_map.MapHandle(address, GetParam());
    EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  hci_spec::AdvertisingHandle old_handle = 0;
  std::optional<AddressTuple> old_address = handle_map.GetAddress(old_handle);
  ASSERT_TRUE(old_address);

  handle_map.RemoveHandle(old_handle);

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});
  std::optional<hci_spec::AdvertisingHandle> new_handle =
      handle_map.MapHandle(address, GetParam());
  EXPECT_LE(new_handle.value(), hci_spec::kMaxAdvertisingHandle);

  ASSERT_TRUE(new_handle);
  ASSERT_EQ(old_handle, new_handle.value());

  std::optional<AddressTuple> new_address =
      handle_map.GetAddress(new_handle.value());
  ASSERT_TRUE(new_address);
  ASSERT_NE(old_address, new_address);
}

TEST_P(AdvertisingHandleMapTest, GetAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  std::optional<AddressTuple> address = handle_map.GetAddress(0);
  EXPECT_FALSE(address);
}

TEST_P(AdvertisingHandleMapTest, RemoveHandle) {
  AdvertisingHandleMap handle_map;
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address, GetParam());
  ASSERT_TRUE(handle);
  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());

  handle_map.RemoveHandle(handle.value());
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
}

TEST_P(AdvertisingHandleMapTest, RemoveHandleNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address, GetParam());
  ASSERT_TRUE(handle);

  size_t size = handle_map.Size();
  handle_map.RemoveHandle(handle.value() + 1);
  EXPECT_EQ(size, handle_map.Size());
}

TEST_P(AdvertisingHandleMapTest, Clear) {
  AdvertisingHandleMap handle_map;
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}),
                           /*extended_pdu=*/false);
  ASSERT_TRUE(handle);

  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());

  handle_map.Clear();
  EXPECT_EQ(0u, handle_map.Size());

  std::optional<AddressTuple> address = handle_map.GetAddress(handle.value());
  EXPECT_FALSE(address);
}

}  // namespace
}  // namespace bt::hci
