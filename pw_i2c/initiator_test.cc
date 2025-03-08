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

#include "pw_i2c/initiator.h"

#include "pw_chrono/system_clock.h"
#include "pw_i2c/message.h"
#include "pw_unit_test/framework.h"

namespace pw::i2c {
namespace {

using Feature = Initiator::Feature;

class TestInitiator : public Initiator {
 public:
  explicit constexpr TestInitiator(Feature supported_features)
      : Initiator(supported_features) {}

  Status DoTransferFor(span<const Message>,
                       chrono::SystemClock::duration) override {
    return pw::OkStatus();
  }
};

constexpr chrono::SystemClock::duration kTimeout =
    std::chrono::milliseconds(100);

TEST(Initiator, FeatureStandard) {
  TestInitiator initiator(Feature::kStandard);

  constexpr Address kAddr = Address::SevenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};
  std::array<std::byte, 1> read_data;

  std::array<Message, 2> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
      Message::ReadMessage(kAddr, read_data),
  };

  PW_TEST_EXPECT_OK(initiator.TransferFor(messages, kTimeout));
}

TEST(Initiator, FeatureNoWriteContinuation) {
  TestInitiator initiator(Feature::kStandard);

  constexpr Address kAddr = Address::SevenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};
  constexpr std::array<std::byte, 1> kWriteData2 = {};

  std::array<Message, 2> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
      Message::WriteMessageContinuation(kWriteData2),
  };
  EXPECT_EQ(initiator.TransferFor(messages, kTimeout), Status::Unimplemented());
}

TEST(Initiator, FeatureNoTenBit) {
  TestInitiator initiator(Feature::kStandard);

  constexpr Address kAddr = Address::TenBit<0xaa>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};

  std::array<Message, 1> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
  };
  EXPECT_EQ(initiator.TransferFor(messages, kTimeout), Status::Unimplemented());
}

TEST(Initiator, FeatureTenBit) {
  TestInitiator initiator(Feature::kStandard | Feature::kTenBit);

  constexpr Address kAddr = Address::TenBit<0xaa>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};

  std::array<Message, 1> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
  };
  PW_TEST_EXPECT_OK(initiator.TransferFor(messages, kTimeout));
}

TEST(Initiator, FeatureNoTenBitSeven) {
  TestInitiator initiator(Feature::kStandard);

  constexpr Address kAddr = Address::TenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};

  std::array<Message, 1> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
  };
  EXPECT_EQ(initiator.TransferFor(messages, kTimeout), Status::Unimplemented());
}

TEST(Initiator, FeatureTenBitSeven) {
  TestInitiator initiator(Feature::kStandard | Feature::kTenBit);

  constexpr Address kAddr = Address::TenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};

  std::array<Message, 1> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
  };
  PW_TEST_EXPECT_OK(initiator.TransferFor(messages, kTimeout));
}

TEST(Initiator, InvalidWriteContinuation) {
  TestInitiator initiator(Feature::kStandard | Feature::kNoStart);

  constexpr Address kAddr = Address::SevenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};
  constexpr std::array<std::byte, 1> kWriteData2 = {};

  std::array<Message, 2> messages = {
      Message::WriteMessageContinuation(kWriteData2),
      Message::WriteMessage(kAddr, kWriteData1),
  };
  EXPECT_EQ(initiator.TransferFor(messages, kTimeout),
            Status::InvalidArgument());
}

TEST(Initiator, ValidWriteContinuation) {
  TestInitiator initiator(Feature::kStandard | Feature::kNoStart);

  constexpr Address kAddr = Address::SevenBit<0x3a>();
  constexpr std::array<std::byte, 1> kWriteData1 = {};
  constexpr std::array<std::byte, 1> kWriteData2 = {};

  std::array<Message, 2> messages = {
      Message::WriteMessage(kAddr, kWriteData1),
      Message::WriteMessageContinuation(kWriteData2),
  };
  PW_TEST_EXPECT_OK(initiator.TransferFor(messages, kTimeout));
}

}  // namespace
}  // namespace pw::i2c
