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
#include "pw_bluetooth_sapphire/internal/host/testing/inspect_util.h"
#include "pw_unit_test/framework.h"

namespace bt::hci {
namespace {

TEST(AdvertisingHandleMapTest, Bidirectional) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address_a = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle_a =
      handle_map.MapHandle(address_a);
  EXPECT_LE(handle_a.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_TRUE(handle_a);

  DeviceAddress address_b = DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  std::optional<hci_spec::AdvertisingHandle> handle_b =
      handle_map.MapHandle(address_b);
  EXPECT_TRUE(handle_b);
  EXPECT_LE(handle_b.value(), hci_spec::kMaxAdvertisingHandle);

  EXPECT_EQ(address_a, handle_map.GetAddress(handle_a.value()));
  EXPECT_EQ(address_b, handle_map.GetAddress(handle_b.value()));
}

TEST(AdvertisingHandleMapTest, MapHandleAlreadyExists) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle_0 =
      handle_map.MapHandle(address);
  EXPECT_LE(handle_0.value(), hci_spec::kMaxAdvertisingHandle);
  ASSERT_TRUE(handle_0);

  std::optional<hci_spec::AdvertisingHandle> handle_1 =
      handle_map.MapHandle(address);
  ASSERT_TRUE(handle_1);
  EXPECT_LE(handle_1.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_NE(handle_0.value(), handle_1.value());
}

TEST(AdvertisingHandleMapTest, MapHandleMoreThanSupported) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<hci_spec::AdvertisingHandle> handle =
        handle_map.MapHandle(address);
    EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});

  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address);
  EXPECT_FALSE(handle);
  EXPECT_EQ(handle_map.capacity(), handle_map.Size());
}

TEST(AdvertisingHandleMapTest, MapHandleSupportHandleReallocation) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<hci_spec::AdvertisingHandle> handle =
        handle_map.MapHandle(address);
    EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  hci_spec::AdvertisingHandle old_handle = 0;
  std::optional<DeviceAddress> old_address = handle_map.GetAddress(old_handle);
  ASSERT_TRUE(old_address);

  handle_map.RemoveHandle(old_handle);

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});
  std::optional<hci_spec::AdvertisingHandle> new_handle =
      handle_map.MapHandle(address);
  EXPECT_LE(new_handle.value(), hci_spec::kMaxAdvertisingHandle);

  ASSERT_TRUE(new_handle);
  ASSERT_EQ(old_handle, new_handle.value());

  std::optional<DeviceAddress> new_address =
      handle_map.GetAddress(new_handle.value());
  ASSERT_TRUE(new_address);
  ASSERT_NE(old_address, new_address);
}

TEST(AdvertisingHandleMapTest, GetAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  std::optional<DeviceAddress> address = handle_map.GetAddress(0);
  EXPECT_FALSE(address);
}

TEST(AdvertisingHandleMapTest, RemoveHandle) {
  AdvertisingHandleMap handle_map;
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address);
  ASSERT_TRUE(handle);
  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());

  handle_map.RemoveHandle(handle.value());
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
}

TEST(AdvertisingHandleMapTest, RemoveHandleNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address);
  ASSERT_TRUE(handle);

  size_t size = handle_map.Size();
  handle_map.RemoveHandle(handle.value() + 1);
  EXPECT_EQ(size, handle_map.Size());
}

TEST(AdvertisingHandleMapTest, Clear) {
  AdvertisingHandleMap handle_map;
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  ASSERT_TRUE(handle);

  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());

  handle_map.Clear();
  EXPECT_EQ(0u, handle_map.Size());

  std::optional<DeviceAddress> address = handle_map.GetAddress(handle.value());
  EXPECT_FALSE(address);
}

#ifndef NINSPECT
TEST(AdvertisingHandleMapTest, Inspect) {
  inspect::Inspector inspector;
  AdvertisingHandleMap handle_map;
  handle_map.AttachInspect(inspector.GetRoot());

  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  ASSERT_TRUE(handle);

  std::optional<uint64_t> inspect_handle =
      bt::testing::GetInspectValue<inspect::UintPropertyValue>(
          inspector,
          {"advertising_handle_map", "advertising_set_0x0", "handle"});
  ASSERT_TRUE(inspect_handle.has_value());
  EXPECT_EQ(inspect_handle, handle);
  std::optional<std::string> address =
      bt::testing::GetInspectValue<inspect::StringPropertyValue>(
          inspector,
          {"advertising_handle_map", "advertising_set_0x0", "address"});
  ASSERT_TRUE(address.has_value());
}
#endif  // NINSPECT

}  // namespace
}  // namespace bt::hci
