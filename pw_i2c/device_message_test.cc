// Copyright 2025 The Pigweed Authors
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

#include <chrono>

#include "pw_bytes/byte_builder.h"
#include "pw_containers/algorithm.h"
#include "pw_i2c/device.h"
#include "pw_i2c/initiator_message_mock.h"
#include "pw_unit_test/framework.h"

using namespace std::literals::chrono_literals;

namespace pw::i2c {
namespace {

constexpr auto kI2cTransactionTimeout = chrono::SystemClock::for_at_least(2ms);

TEST(Device, WriteReadForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();

  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3>();
  constexpr auto kExpectedRead1 = bytes::Array<1, 2>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(OkStatus(), kTestDeviceAddress, kExpectedWrite1),
              MockReadMessage(OkStatus(), kTestDeviceAddress, kExpectedRead1),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);

  Device device = Device(initiator, kTestDeviceAddress);

  std::array<std::byte, kExpectedRead1.size()> read1;
  EXPECT_EQ(device.WriteReadFor(kExpectedWrite1, read1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read1, kExpectedRead1));
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, WriteForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();

  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(OkStatus(), kTestDeviceAddress, kExpectedWrite1),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  EXPECT_EQ(device.WriteFor(kExpectedWrite1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, ReadForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();

  constexpr auto kExpectedRead1 = bytes::Array<1, 2, 3>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockReadMessage(OkStatus(), kTestDeviceAddress, kExpectedRead1),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  std::array<std::byte, kExpectedRead1.size()> read1;
  EXPECT_EQ(device.ReadFor(read1, kI2cTransactionTimeout), OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, ProbeDeviceForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockProbeMessage(OkStatus(), kTestDeviceAddress),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  EXPECT_EQ(
      initiator.ProbeDeviceFor(kTestDeviceAddress, kI2cTransactionTimeout),
      OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, UpdateAddressWriteReadForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();
  constexpr Address kTestDeviceAddressNew = Address::SevenBit<0x41>();

  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3>();
  constexpr auto kExpectedRead1 = bytes::Array<1, 2>();
  constexpr auto kExpectedWrite2 = bytes::Array<4, 5, 6>();
  constexpr auto kExpectedRead2 = bytes::Array<3, 4>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(OkStatus(), kTestDeviceAddress, kExpectedWrite1),
              MockReadMessage(OkStatus(), kTestDeviceAddress, kExpectedRead1),
          }),
          kI2cTransactionTimeout),
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(
                  OkStatus(), kTestDeviceAddressNew, kExpectedWrite2),
              MockReadMessage(
                  OkStatus(), kTestDeviceAddressNew, kExpectedRead2),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);

  Device device = Device(initiator, kTestDeviceAddress);

  std::array<std::byte, kExpectedRead1.size()> read1;
  EXPECT_EQ(device.WriteReadFor(kExpectedWrite1, read1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read1, kExpectedRead1));
  device.UpdateAddress(kTestDeviceAddressNew);
  std::array<std::byte, kExpectedRead2.size()> read2;
  EXPECT_EQ(device.WriteReadFor(kExpectedWrite2, read2, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read2, kExpectedRead2));
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, UpdateAddressWriteForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();
  constexpr Address kTestDeviceAddressNew = Address::SevenBit<0x41>();

  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3>();
  constexpr auto kExpectedWrite2 = bytes::Array<4, 5, 6>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(OkStatus(), kTestDeviceAddress, kExpectedWrite1),
          }),
          kI2cTransactionTimeout),
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockWriteMessage(
                  OkStatus(), kTestDeviceAddressNew, kExpectedWrite2),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  EXPECT_EQ(device.WriteFor(kExpectedWrite1, kI2cTransactionTimeout),
            OkStatus());
  device.UpdateAddress(kTestDeviceAddressNew);
  EXPECT_EQ(device.WriteFor(kExpectedWrite2, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, UpdateAddressReadForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();
  constexpr Address kTestDeviceAddressNew = Address::SevenBit<0x41>();

  constexpr auto kExpectedRead1 = bytes::Array<1, 2, 3>();
  constexpr auto kExpectedRead2 = bytes::Array<4, 5, 6>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockReadMessage(OkStatus(), kTestDeviceAddress, kExpectedRead1),
          }),
          kI2cTransactionTimeout),
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockReadMessage(
                  OkStatus(), kTestDeviceAddressNew, kExpectedRead2),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  std::array<std::byte, kExpectedRead1.size()> read1;
  EXPECT_EQ(device.ReadFor(read1, kI2cTransactionTimeout), OkStatus());
  device.UpdateAddress(kTestDeviceAddressNew);
  std::array<std::byte, kExpectedRead2.size()> read2;
  EXPECT_EQ(device.ReadFor(read2, kI2cTransactionTimeout), OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

TEST(Device, UpdateAddressProbeDeviceForOk) {
  constexpr Address kTestDeviceAddress = Address::SevenBit<0x3F>();
  constexpr Address kTestDeviceAddressNew = Address::SevenBit<0x41>();

  auto expected_transactions = MakeExpectedTransactionArray({
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockProbeMessage(OkStatus(), kTestDeviceAddress),
          }),
          kI2cTransactionTimeout),
      MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({
              MockProbeMessage(OkStatus(), kTestDeviceAddressNew),
          }),
          kI2cTransactionTimeout),
  });

  MockMessageInitiator initiator(expected_transactions);
  Device device = Device(initiator, kTestDeviceAddress);

  EXPECT_EQ(
      initiator.ProbeDeviceFor(kTestDeviceAddress, kI2cTransactionTimeout),
      OkStatus());
  device.UpdateAddress(kTestDeviceAddressNew);
  EXPECT_EQ(
      initiator.ProbeDeviceFor(kTestDeviceAddressNew, kI2cTransactionTimeout),
      OkStatus());
  EXPECT_EQ(initiator.Finalize(), OkStatus());
}

}  // namespace
}  // namespace pw::i2c
