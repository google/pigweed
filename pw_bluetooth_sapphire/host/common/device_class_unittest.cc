// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/device_class.h"

#include "gtest/gtest.h"

namespace bt {
namespace {

struct TestPayload {
  uint8_t arg0;
  DeviceClass class_of_device;
} __attribute__((packed));

TEST(DeviceClassTest, CastFromBytes) {
  std::array<uint8_t, 4> bytes{{10, 0x06, 0x02, 0x02}};
  EXPECT_EQ(bytes.size(), sizeof(TestPayload));

  auto* test_payload = reinterpret_cast<TestPayload*>(bytes.data());
  EXPECT_EQ(10, test_payload->arg0);
  EXPECT_EQ(DeviceClass::MajorClass::kPhone, test_payload->class_of_device.major_class());
  std::unordered_set<DeviceClass::ServiceClass> srvs_expected = {
      DeviceClass::ServiceClass::kNetworking};
  EXPECT_EQ(srvs_expected, test_payload->class_of_device.GetServiceClasses());

  // Computer -- Laptop with no Serivces.
  std::array<uint8_t, 4> no_srv_bytes{{0xBA, 0x0C, 0x01, 0x00}};
  EXPECT_EQ(no_srv_bytes.size(), sizeof(TestPayload));

  test_payload = reinterpret_cast<TestPayload*>(no_srv_bytes.data());
  EXPECT_EQ(0xBA, test_payload->arg0);
  EXPECT_EQ(DeviceClass::MajorClass::kComputer, test_payload->class_of_device.major_class());
  srvs_expected = {};
  EXPECT_EQ(srvs_expected, test_payload->class_of_device.GetServiceClasses());

  // Wearable -- watch with Location and Audio services
  std::array<uint8_t, 4> two_srv_bytes{{0xA0, 0x04, 0x07, 0x21}};
  EXPECT_EQ(two_srv_bytes.size(), sizeof(TestPayload));

  test_payload = reinterpret_cast<TestPayload*>(two_srv_bytes.data());
  EXPECT_EQ(0xA0, test_payload->arg0);
  EXPECT_EQ(DeviceClass::MajorClass::kWearable, test_payload->class_of_device.major_class());
  srvs_expected = {DeviceClass::ServiceClass::kAudio, DeviceClass::ServiceClass::kPositioning};
  EXPECT_EQ(srvs_expected, test_payload->class_of_device.GetServiceClasses());
}

TEST(DeviceClassTest, ConstructFromUInt32) {
  // AudioVideo -- headset with Rendering and Audio services
  DeviceClass class_of_device(0x240404);

  EXPECT_EQ(DeviceClass::MajorClass::kAudioVideo, class_of_device.major_class());

  const uint8_t WEARABLE_HEADSET_DEVICE_MINOR_CLASS = 1;
  EXPECT_EQ(WEARABLE_HEADSET_DEVICE_MINOR_CLASS, class_of_device.minor_class());

  std::unordered_set<DeviceClass::ServiceClass> srvs_expected = {
      DeviceClass::ServiceClass::kAudio, DeviceClass::ServiceClass::kRendering};
  EXPECT_EQ(srvs_expected, class_of_device.GetServiceClasses());
}

TEST(DeviceClassTest, ToString) {
  DeviceClass device;
  EXPECT_EQ("Unspecified", device.ToString());

  device = DeviceClass({0x06, 0x02, 0x02});
  EXPECT_EQ("Phone (Networking)", device.ToString());

  device = DeviceClass({0x06, 0x02, 0x60});
  EXPECT_EQ("Phone (Telephony, Audio)", device.ToString());
}

TEST(DeviceClassTest, Comparison) {
  DeviceClass class1(DeviceClass::MajorClass::kPhone);
  DeviceClass class2(DeviceClass::MajorClass::kPhone);
  DeviceClass class3(DeviceClass::MajorClass::kComputer);
  EXPECT_EQ(class1, class2);
  EXPECT_NE(class2, class3);
}

}  // namespace
}  // namespace bt
