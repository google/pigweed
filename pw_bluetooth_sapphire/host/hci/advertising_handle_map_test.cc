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
  std::optional<AdvertisementId> adv_id_a = handle_map.Insert(address_a);
  ASSERT_TRUE(adv_id_a);

  DeviceAddress address_b = DeviceAddress(DeviceAddress::Type::kLEPublic, {1});
  std::optional<AdvertisementId> adv_id_b = handle_map.Insert(address_b);
  ASSERT_TRUE(adv_id_b);

  EXPECT_NE(adv_id_a.value(), adv_id_b.value());
  EXPECT_EQ(address_a, handle_map.GetAddress(adv_id_a.value()));
  EXPECT_EQ(address_b, handle_map.GetAddress(adv_id_b.value()));

  hci_spec::AdvertisingHandle handle_a = handle_map.GetHandle(adv_id_a.value());
  EXPECT_LT(handle_a, hci_spec::kMaxAdvertisingHandle);
  hci_spec::AdvertisingHandle handle_b = handle_map.GetHandle(adv_id_b.value());
  EXPECT_LT(handle_b, hci_spec::kMaxAdvertisingHandle);
  EXPECT_NE(handle_a, handle_b);

  std::optional<AdvertisementId> adv_id_a_from_handle =
      handle_map.GetId(handle_a);
  ASSERT_TRUE(adv_id_a_from_handle.has_value());
  EXPECT_EQ(adv_id_a.value(), adv_id_a_from_handle.value());
  std::optional<AdvertisementId> adv_id_b_from_handle =
      handle_map.GetId(handle_b);
  ASSERT_TRUE(adv_id_b_from_handle.has_value());
  EXPECT_EQ(adv_id_b.value(), adv_id_b_from_handle.value());
}

TEST(AdvertisingHandleMapTest, InsertAddressAlreadyExists) {
  AdvertisingHandleMap handle_map;

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisementId> adv_id_a = handle_map.Insert(address);
  ASSERT_TRUE(adv_id_a);

  std::optional<AdvertisementId> adv_id_b = handle_map.Insert(address);
  ASSERT_TRUE(adv_id_b);

  hci_spec::AdvertisingHandle handle_a = handle_map.GetHandle(adv_id_a.value());
  hci_spec::AdvertisingHandle handle_b = handle_map.GetHandle(adv_id_b.value());
  EXPECT_NE(handle_a, handle_b);
  EXPECT_LE(handle_a, hci_spec::kMaxAdvertisingHandle);
  EXPECT_LE(handle_b, hci_spec::kMaxAdvertisingHandle);
}

TEST(AdvertisingHandleMapTest, InsertMoreThanSupported) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<AdvertisementId> adv_id = handle_map.Insert(address);
    ASSERT_TRUE(adv_id) << "Couldn't insert device address " << i;
    hci_spec::AdvertisingHandle handle = handle_map.GetHandle(adv_id.value());
    EXPECT_LE(handle, hci_spec::kMaxAdvertisingHandle);
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});

  std::optional<AdvertisementId> adv_id = handle_map.Insert(address);
  EXPECT_FALSE(adv_id);
  EXPECT_EQ(handle_map.capacity(), handle_map.Size());
}

TEST(AdvertisingHandleMapTest, MapHandleSupportHandleReallocation) {
  AdvertisingHandleMap handle_map;

  for (uint8_t i = 0; i < handle_map.capacity(); i++) {
    DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {i});
    std::optional<AdvertisementId> adv_id = handle_map.Insert(address);
    ASSERT_TRUE(adv_id) << "Couldn't insert device address " << i;
    hci_spec::AdvertisingHandle handle = handle_map.GetHandle(adv_id.value());
    EXPECT_LE(handle, hci_spec::kMaxAdvertisingHandle);
    EXPECT_EQ(i + 1u, handle_map.Size());
  }

  hci_spec::AdvertisingHandle old_handle = 0;
  std::optional<AdvertisementId> old_id = handle_map.GetId(old_handle);
  ASSERT_TRUE(old_id);

  handle_map.Erase(old_id.value());

  DeviceAddress address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {handle_map.capacity()});
  std::optional<AdvertisementId> new_adv_id = handle_map.Insert(address);
  ASSERT_TRUE(new_adv_id);
  EXPECT_NE(old_id.value(), new_adv_id.value());
  hci_spec::AdvertisingHandle new_handle =
      handle_map.GetHandle(new_adv_id.value());
  EXPECT_EQ(new_handle, old_handle);
}

TEST(AdvertisingHandleMapTest, GetAddressNonExistent) {
  AdvertisingHandleMap handle_map;
  ASSERT_DEATH_IF_SUPPORTED(handle_map.GetAddress(AdvertisementId(0)),
                            ".*iter.*");
}

TEST(AdvertisingHandleMapTest, Erase) {
  AdvertisingHandleMap handle_map;
  EXPECT_TRUE(handle_map.Empty());

  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisementId> adv_id = handle_map.Insert(address);
  ASSERT_TRUE(adv_id);
  EXPECT_EQ(1u, handle_map.Size());
  EXPECT_FALSE(handle_map.Empty());
  handle_map.Erase(adv_id.value());
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_TRUE(handle_map.Empty());
}

TEST(AdvertisingHandleMapTest, EraseNonExistent) {
  AdvertisingHandleMap handle_map;
  DeviceAddress address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  std::optional<AdvertisementId> adv_id = handle_map.Insert(address);
  ASSERT_TRUE(adv_id);

  EXPECT_EQ(handle_map.Size(), 1u);
  handle_map.Erase(AdvertisementId(adv_id->value() + 1));
  EXPECT_EQ(handle_map.Size(), 1u);
}

TEST(AdvertisingHandleMapTest, Clear) {
  AdvertisingHandleMap handle_map;
  std::optional<AdvertisementId> adv_id =
      handle_map.Insert(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  ASSERT_TRUE(adv_id);
  EXPECT_EQ(1u, handle_map.Size());
  hci_spec::AdvertisingHandle handle = handle_map.GetHandle(adv_id.value());

  handle_map.Clear();
  EXPECT_EQ(0u, handle_map.Size());
  EXPECT_FALSE(handle_map.GetId(handle));

  EXPECT_DEATH_IF_SUPPORTED(handle_map.GetAddress(adv_id.value()), ".*iter.*");
}

#ifndef NINSPECT
TEST(AdvertisingHandleMapTest, Inspect) {
  inspect::Inspector inspector;
  AdvertisingHandleMap handle_map;
  handle_map.AttachInspect(inspector.GetRoot());

  std::optional<AdvertisementId> adv_id =
      handle_map.Insert(DeviceAddress(DeviceAddress::Type::kLEPublic, {0}));
  ASSERT_TRUE(adv_id);

  std::optional<uint64_t> inspect_handle =
      bt::testing::GetInspectValue<inspect::UintPropertyValue>(
          inspector,
          {"advertising_handle_map", "advertising_set_0x0", "handle"});
  ASSERT_TRUE(inspect_handle.has_value());
  std::optional<std::string> address =
      bt::testing::GetInspectValue<inspect::StringPropertyValue>(
          inspector,
          {"advertising_handle_map", "advertising_set_0x0", "address"});
  ASSERT_TRUE(address.has_value());
  std::optional<std::string> id =
      bt::testing::GetInspectValue<inspect::StringPropertyValue>(
          inspector, {"advertising_handle_map", "advertising_set_0x0", "id"});
  ASSERT_TRUE(id.has_value());
}
#endif  // NINSPECT

}  // namespace
}  // namespace bt::hci
