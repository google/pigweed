// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "advertising_handle_map.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"

namespace bt::hci {
namespace {

TEST(AdvertisingHandleMap, Bidirectional) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address_a = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisingHandle> handle_a = handle_map.MapHandle(address_a);
  EXPECT_LE(handle_a.value(), kMaxAdvertisingHandle);
  EXPECT_TRUE(handle_a);

  DeviceAddress address_b = DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  std::optional<AdvertisingHandle> handle_b = handle_map.MapHandle(address_b);
  EXPECT_TRUE(handle_b);
  EXPECT_LE(handle_b.value(), kMaxAdvertisingHandle);

  EXPECT_EQ(address_a, handle_map.GetAddress(handle_a.value()));
  EXPECT_EQ(address_b, handle_map.GetAddress(handle_b.value()));
}

TEST(AdvertisingHandleMap, GetHandleAlreadyExists) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisingHandle> expected = handle_map.MapHandle(address);
  EXPECT_LE(expected.value(), kMaxAdvertisingHandle);
  ASSERT_TRUE(expected);

  std::optional<AdvertisingHandle> actual = handle_map.MapHandle(address);
  EXPECT_LE(actual.value(), kMaxAdvertisingHandle);
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected, actual);
}

TEST(AdvertisingHandleMap, GetHandleMoreThanSupported) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < AdvertisingHandleMap::kMaxElements; i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
    EXPECT_LE(handle.value(), kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {AdvertisingHandleMap::kMaxElements});

  std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
  EXPECT_FALSE(handle);
  EXPECT_EQ(AdvertisingHandleMap::kMaxElements, handle_map.Size());
}

TEST(AdvertisingHandleMap, GetHandleSupportHandleReallocation) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < AdvertisingHandleMap::kMaxElements; i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
    EXPECT_LE(handle.value(), kMaxAdvertisingHandle);
    EXPECT_TRUE(handle) << "Couldn't add device address " << i;
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  AdvertisingHandle old_handle = 0;
  std::optional<DeviceAddress> old_address = handle_map.GetAddress(old_handle);
  ASSERT_TRUE(old_address);

  handle_map.RemoveHandle(old_handle);

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {AdvertisingHandleMap::kMaxElements});
  std::optional<AdvertisingHandle> new_handle = handle_map.MapHandle(address);
  EXPECT_LE(new_handle.value(), kMaxAdvertisingHandle);

  ASSERT_TRUE(new_handle);
  ASSERT_EQ(old_handle, new_handle.value());

  std::optional<DeviceAddress> new_address = handle_map.GetAddress(new_handle.value());
  ASSERT_TRUE(new_address);
  ASSERT_NE(old_address, new_address);
}

TEST(AdvertisingHandleMap, GetAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  std::optional<DeviceAddress> address = handle_map.GetAddress(0);
  EXPECT_FALSE(address);
}

TEST(AdvertisingHandleMap, RemoveHandle) {
  AdvertisingHandleMap handle_map;
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
  EXPECT_LE(handle.value(), kMaxAdvertisingHandle);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());

  handle_map.RemoveHandle(handle.value());
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
}

TEST(AdvertisingHandleMap, RemoveAddress) {
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

TEST(AdvertisingHandleMap, RemoveHandleNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
  ASSERT_TRUE(handle);

  size_t size = handle_map.Size();

  handle_map.RemoveHandle(handle.value() + 1);

  EXPECT_EQ(size, handle_map.Size());
  handle = handle_map.MapHandle(address);
  EXPECT_TRUE(handle);
}

TEST(AdvertisingHandleMap, RemoveAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisingHandle> handle = handle_map.MapHandle(address);
  ASSERT_TRUE(handle);

  size_t size = handle_map.Size();

  DeviceAddress nonexistent_address = DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  handle_map.RemoveAddress(nonexistent_address);

  EXPECT_EQ(size, handle_map.Size());
  handle = handle_map.MapHandle(address);
  EXPECT_TRUE(handle);
}

TEST(AdvertisingHandleMap, PeekNextHandle) {
  AdvertisingHandleMap handle_map;
  std::optional<hci::AdvertisingHandle> next_handle = handle_map.PeekNextHandle();
  EXPECT_TRUE(next_handle);

  handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  std::optional<DeviceAddress> address = handle_map.GetAddress(next_handle.value());
  EXPECT_TRUE(address);
  EXPECT_NE(next_handle, handle_map.PeekNextHandle());
}

TEST(AdvertisingHandleMap, Clear) {
  AdvertisingHandleMap handle_map;
  std::optional<AdvertisingHandle> handle =
      handle_map.MapHandle(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  EXPECT_LE(handle.value(), kMaxAdvertisingHandle);
  EXPECT_TRUE(handle);
  EXPECT_EQ(1u, handle_map.Size());

  handle_map.Clear();
  EXPECT_EQ(0u, handle_map.Size());

  std::optional<DeviceAddress> address = handle_map.GetAddress(handle.value());
  EXPECT_FALSE(address);
}

}  // namespace
}  // namespace bt::hci
