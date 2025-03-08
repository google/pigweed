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

#include "pw_i2c/initiator_message_mock.h"

#include <array>
#include <chrono>

#include "pw_bytes/array.h"
#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/algorithm.h"
#include "pw_containers/vector.h"
#include "pw_i2c/address.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

using namespace std::literals::chrono_literals;

namespace pw::i2c {
namespace {

constexpr auto kI2cTransactionTimeout = chrono::SystemClock::for_at_least(2ms);

TEST(Transaction, Read) {
  constexpr Address kAddress1 = Address::SevenBit<0x01>();
  constexpr auto kExpectRead1 = bytes::Array<1, 2, 3, 4, 5>();

  constexpr Address kAddress2 = Address::SevenBit<0x02>();
  constexpr auto kExpectRead2 = bytes::Array<3, 4, 5>();

  auto expected_transactions = MakeExpectedTransactionArray(
      {MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockReadMessage(OkStatus(), kAddress1, kExpectRead1),
           }),
           kI2cTransactionTimeout),

       MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockReadMessage(OkStatus(), kAddress2, kExpectRead2),
           }),
           kI2cTransactionTimeout)});

  MockMessageInitiator mocked_i2c(expected_transactions);

  std::array<std::byte, kExpectRead1.size()> read1;
  EXPECT_EQ(mocked_i2c.ReadFor(kAddress1, read1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read1, kExpectRead1));

  std::array<std::byte, kExpectRead2.size()> read2;
  EXPECT_EQ(mocked_i2c.ReadFor(kAddress2, read2, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read2, kExpectRead2));

  EXPECT_EQ(mocked_i2c.Finalize(), OkStatus());
}

TEST(Transaction, Write) {
  static constexpr Address kAddress1 = Address::SevenBit<0x01>();
  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3, 4, 5>();

  static constexpr Address kAddress2 = Address::SevenBit<0x02>();
  constexpr auto kExpectedWrite2 = bytes::Array<3, 4, 5>();

  auto expected_transactions = MakeExpectedTransactionArray(
      {MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockWriteMessage(OkStatus(), kAddress1, kExpectedWrite1),
           }),
           kI2cTransactionTimeout),

       MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockWriteMessage(OkStatus(), kAddress2, kExpectedWrite2),
           }),
           kI2cTransactionTimeout)});

  MockMessageInitiator mocked_i2c(expected_transactions);

  EXPECT_EQ(
      mocked_i2c.WriteFor(kAddress1, kExpectedWrite1, kI2cTransactionTimeout),
      OkStatus());

  EXPECT_EQ(
      mocked_i2c.WriteFor(kAddress2, kExpectedWrite2, kI2cTransactionTimeout),
      OkStatus());

  EXPECT_EQ(mocked_i2c.Finalize(), OkStatus());
}

TEST(Transaction, WriteRead) {
  static constexpr Address kAddress1 = Address::SevenBit<0x01>();
  constexpr auto kExpectedWrite1 = bytes::Array<1, 2, 3, 4, 5>();
  constexpr auto kExpectedRead1 = bytes::Array<1, 2>();

  static constexpr Address kAddress2 = Address::SevenBit<0x02>();
  constexpr auto kExpectedWrite2 = bytes::Array<3, 4, 5>();
  constexpr const auto kExpectedRead2 = bytes::Array<3, 4>();

  auto expected_transactions = MakeExpectedTransactionArray(
      {MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockWriteMessage(OkStatus(), kAddress1, kExpectedWrite1),
               MockReadMessage(OkStatus(), kAddress1, kExpectedRead1),
           }),
           kI2cTransactionTimeout),

       MockMessageTransaction(
           OkStatus(),
           MakeExpectedMessageArray({
               MockWriteMessage(OkStatus(), kAddress2, kExpectedWrite2),
               MockReadMessage(OkStatus(), kAddress2, kExpectedRead2),
           }),
           kI2cTransactionTimeout)});

  MockMessageInitiator mocked_i2c(expected_transactions);

  std::array<std::byte, kExpectedRead1.size()> read1;
  EXPECT_EQ(mocked_i2c.WriteReadFor(
                kAddress1, kExpectedWrite1, read1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read1, kExpectedRead1));

  std::array<std::byte, kExpectedRead1.size()> read2;
  EXPECT_EQ(mocked_i2c.WriteReadFor(
                kAddress2, kExpectedWrite2, read2, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_TRUE(pw::containers::Equal(read2, kExpectedRead2));

  EXPECT_EQ(mocked_i2c.Finalize(), OkStatus());
}

TEST(Transaction, Probe) {
  static constexpr Address kAddress1 = Address::SevenBit<0x01>();

  auto expected_transactions =
      MakeExpectedTransactionArray({MockMessageTransaction(
          OkStatus(),
          MakeExpectedMessageArray({MockProbeMessage(OkStatus(), kAddress1)}),
          kI2cTransactionTimeout)});

  MockMessageInitiator mock_initiator(expected_transactions);

  EXPECT_EQ(mock_initiator.ProbeDeviceFor(kAddress1, kI2cTransactionTimeout),
            OkStatus());
  EXPECT_EQ(mock_initiator.Finalize(), OkStatus());
}

}  // namespace
}  // namespace pw::i2c
