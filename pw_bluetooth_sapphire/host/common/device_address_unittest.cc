// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"

#include <map>
#include <unordered_map>

#include "gtest/gtest.h"

namespace bt {
namespace common {
namespace {

const DeviceAddress kClassic(DeviceAddress::Type::kBREDR, "41:11:22:33:44:55");
const DeviceAddress kPublic(DeviceAddress::Type::kLEPublic,
                            "42:11:22:33:44:55");
const DeviceAddress kNonResolvable(DeviceAddress::Type::kLERandom,
                                   "00:11:22:33:44:55");
const DeviceAddress kResolvable(DeviceAddress::Type::kLERandom,
                                "43:11:22:33:44:55");
const DeviceAddress kStatic(DeviceAddress::Type::kLERandom,
                            "C3:11:22:33:44:55");

struct TestPayload {
  uint8_t arg0;
  DeviceAddressBytes bdaddr;
} __attribute__((packed));

TEST(DeviceAddressBytesTest, ToString) {
  DeviceAddressBytes bdaddr({1, 15, 2, 255, 127, 3});
  EXPECT_EQ("03:7F:FF:02:0F:01", bdaddr.ToString());

  bdaddr = DeviceAddressBytes();
  EXPECT_EQ("00:00:00:00:00:00", bdaddr.ToString());
}

TEST(DeviceAddressBytesTest, SetFromString) {
  DeviceAddressBytes bdaddr;
  EXPECT_FALSE(bdaddr.SetFromString(""));
  EXPECT_FALSE(bdaddr.SetFromString("FF"));
  EXPECT_FALSE(bdaddr.SetFromString("FF:FF:FF:FF:"));
  EXPECT_FALSE(bdaddr.SetFromString("FF:FF:FF:FF:FF:F"));
  EXPECT_FALSE(bdaddr.SetFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(bdaddr.SetFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(bdaddr.SetFromString("FF:FF:FF:FF:FF:FF "));
  EXPECT_FALSE(bdaddr.SetFromString(" FF:FF:FF:FF:FF:FF"));

  EXPECT_TRUE(bdaddr.SetFromString("FF:FF:FF:FF:FF:FF"));
  EXPECT_EQ("FF:FF:FF:FF:FF:FF", bdaddr.ToString());

  EXPECT_TRUE(bdaddr.SetFromString("03:7F:FF:02:0F:01"));
  EXPECT_EQ("03:7F:FF:02:0F:01", bdaddr.ToString());

  // Test the constructor with a valid string (an invalid one would fail
  // fatally).
  bdaddr = DeviceAddressBytes("03:7F:FF:02:0F:01");
  EXPECT_EQ("03:7F:FF:02:0F:01", bdaddr.ToString());
}

TEST(DeviceAddressBytesTest, CastFromBytes) {
  std::array<uint8_t, 7> bytes{{10, 1, 15, 2, 255, 127, 3}};
  EXPECT_EQ(bytes.size(), sizeof(TestPayload));

  auto* bdaddr = reinterpret_cast<DeviceAddressBytes*>(bytes.data());
  EXPECT_EQ("7F:FF:02:0F:01:0A", bdaddr->ToString());

  auto* test_payload = reinterpret_cast<TestPayload*>(bytes.data());
  EXPECT_EQ(10, test_payload->arg0);
  EXPECT_EQ("03:7F:FF:02:0F:01", test_payload->bdaddr.ToString());
}

TEST(DeviceAddressBytesTest, Comparison) {
  DeviceAddressBytes bdaddr0, bdaddr1;
  EXPECT_EQ(bdaddr0, bdaddr1);

  bdaddr0 = DeviceAddressBytes({1, 2, 3, 4, 5, 6});
  EXPECT_NE(bdaddr0, bdaddr1);

  bdaddr1 = bdaddr0;
  EXPECT_EQ(bdaddr0, bdaddr1);
}

TEST(DeviceAddressTest, Map) {
  std::map<DeviceAddress, int> map;

  DeviceAddress address1;
  DeviceAddress address2(address1);
  DeviceAddress address3(DeviceAddress::Type::kLEPublic, address1.value());
  DeviceAddress address4(DeviceAddress::Type::kLEPublic, "00:00:00:00:00:01");

  map[address1] = 1;

  auto iter = map.find(address1);
  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(1, iter->second);

  iter = map.find(address2);
  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(1, iter->second);

  iter = map.find(address3);
  EXPECT_EQ(map.end(), iter);
  iter = map.find(address4);
  EXPECT_EQ(map.end(), iter);

  map[address3] = 2;
  map[address4] = 3;

  EXPECT_EQ(3u, map.size());
  EXPECT_EQ(2, map[address3]);
  EXPECT_EQ(3, map[address4]);
}

TEST(DeviceAddressTest, UnorderedMap) {
  std::unordered_map<DeviceAddress, int> map;

  DeviceAddress address1;
  DeviceAddress address2(address1);
  DeviceAddress address3(DeviceAddress::Type::kLEPublic, address1.value());
  DeviceAddress address4(DeviceAddress::Type::kLEPublic, "00:00:00:00:00:01");

  map[address1] = 1;

  auto iter = map.find(address1);
  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(1, iter->second);

  iter = map.find(address2);
  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(1, iter->second);

  iter = map.find(address3);
  EXPECT_EQ(map.end(), iter);
  iter = map.find(address4);
  EXPECT_EQ(map.end(), iter);

  map[address3] = 2;
  map[address4] = 3;

  EXPECT_EQ(3u, map.size());
  EXPECT_EQ(2, map[address3]);
  EXPECT_EQ(3, map[address4]);
}

TEST(DeviceAddressTest, IsResolvablePrivate) {
  EXPECT_FALSE(kClassic.IsResolvablePrivate());
  EXPECT_FALSE(kPublic.IsResolvablePrivate());
  EXPECT_FALSE(kNonResolvable.IsResolvablePrivate());
  EXPECT_TRUE(kResolvable.IsResolvablePrivate());
  EXPECT_FALSE(kStatic.IsResolvablePrivate());
}

TEST(DeviceAddressTest, IsNonResolvablePrivate) {
  EXPECT_FALSE(kClassic.IsNonResolvablePrivate());
  EXPECT_FALSE(kPublic.IsNonResolvablePrivate());
  EXPECT_TRUE(kNonResolvable.IsNonResolvablePrivate());
  EXPECT_FALSE(kResolvable.IsNonResolvablePrivate());
  EXPECT_FALSE(kStatic.IsNonResolvablePrivate());
}

TEST(DeviceAddressTest, IsStatic) {
  EXPECT_FALSE(kClassic.IsStaticRandom());
  EXPECT_FALSE(kPublic.IsStaticRandom());
  EXPECT_FALSE(kNonResolvable.IsStaticRandom());
  EXPECT_FALSE(kResolvable.IsStaticRandom());
  EXPECT_TRUE(kStatic.IsStaticRandom());
}

}  // namespace
}  // namespace common
}  // namespace bt
