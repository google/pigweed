// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_handle_map.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

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

TEST(AdvertisingHandleMapTest, GetHandleDoesntCreateMapping) {
  AdvertisingHandleMap handle_map;
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.GetHandle(address);
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
  EXPECT_FALSE(handle);

  handle = handle_map.MapHandle(address);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());
  EXPECT_TRUE(handle);
  EXPECT_EQ(0u, handle.value());

  handle = handle_map.GetHandle(address);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());
  EXPECT_TRUE(handle);
}

TEST(AdvertisingHandleMapTest, MapHandleAlreadyExists) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> expected =
      handle_map.MapHandle(address);
  EXPECT_LE(expected.value(), hci_spec::kMaxAdvertisingHandle);
  ASSERT_TRUE(expected);

  std::optional<hci_spec::AdvertisingHandle> actual =
      handle_map.MapHandle(address);
  EXPECT_LE(actual.value(), hci_spec::kMaxAdvertisingHandle);
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected, actual);
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
  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());

  handle_map.RemoveHandle(handle.value());
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
}

TEST(AdvertisingHandleMapTest, RemoveAddress) {
  AdvertisingHandleMap handle_map;
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  handle_map.MapHandle(address);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());

  handle_map.RemoveAddress(address);
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
  handle = handle_map.MapHandle(address);
  EXPECT_TRUE(handle);
}

TEST(AdvertisingHandleMapTest, RemoveAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(address);
  ASSERT_TRUE(handle);

  size_t size = handle_map.Size();

  DeviceAddress nonexistent_address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  handle_map.RemoveAddress(nonexistent_address);

  EXPECT_EQ(size, handle_map.Size());
  handle = handle_map.MapHandle(address);
  EXPECT_TRUE(handle);
}

TEST(AdvertisingHandleMapTest, Clear) {
  AdvertisingHandleMap handle_map;
  std::optional<hci_spec::AdvertisingHandle> handle =
      handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  EXPECT_LE(handle.value(), hci_spec::kMaxAdvertisingHandle);
  EXPECT_TRUE(handle);
  EXPECT_EQ(1u, handle_map.Size());

  handle_map.Clear();
  EXPECT_EQ(0u, handle_map.Size());

  std::optional<DeviceAddress> address = handle_map.GetAddress(handle.value());
  EXPECT_FALSE(address);
}

}  // namespace
}  // namespace bt::hci
