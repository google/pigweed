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
#include "pw_i2c/i2c_service.h"

#include <algorithm>
#include <chrono>

#include "gtest/gtest.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_i2c/initiator_mock.h"
#include "pw_rpc/pwpb/test_method_context.h"
#include "pw_status/status.h"

namespace pw::i2c {
namespace {

auto MakeSingletonSelector(Initiator* initiator) {
  return [initiator](size_t pos) { return pos == 0 ? initiator : nullptr; };
}

TEST(I2cServiceTest, I2cWriteSingleByteOk) {
  Vector<std::byte, 4> register_addr{};
  Vector<std::byte, 4> register_value{};
  constexpr auto kExpectWrite = bytes::Array<0x02, 0x03>();
  register_addr.push_back(kExpectWrite[0]);
  register_value.push_back(kExpectWrite[1]);
  auto transactions = MakeExpectedTransactionArray(
      {Transaction(OkStatus(),
                   Address{0x01},
                   kExpectWrite,
                   {},
                   std::chrono::milliseconds(100))});
  MockInitiator i2c_initiator(transactions);

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cWrite)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 0,
                .target_address = 0x01,
                .register_address = register_addr,
                .value = register_value});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), OkStatus());
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cWriteMultiByteOk) {
  constexpr int kWriteSize = 4;
  Vector<std::byte, kWriteSize> register_addr{};
  Vector<std::byte, kWriteSize> register_value{};
  constexpr auto kExpectWrite =
      bytes::Array<0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09>();
  std::copy(kExpectWrite.begin(),
            kExpectWrite.begin() + kWriteSize,
            std::back_inserter(register_addr));
  std::copy(kExpectWrite.begin() + kWriteSize,
            kExpectWrite.end(),
            std::back_inserter(register_value));
  auto transactions = MakeExpectedTransactionArray(
      {Transaction(OkStatus(),
                   Address{0x01},
                   kExpectWrite,
                   {},
                   std::chrono::milliseconds(100))});
  MockInitiator i2c_initiator(transactions);

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cWrite)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 0,
                .target_address = 0x01,
                .register_address = register_addr,
                .value = register_value});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), OkStatus());
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cWriteInvalidBusIndex) {
  Vector<std::byte, 4> register_addr{};
  Vector<std::byte, 4> register_value{};

  MockInitiator i2c_initiator({});

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cWrite)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 1,
                .target_address = 0x01,
                .register_address = register_addr,
                .value = register_value});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), Status::InvalidArgument());
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cReadSingleByteOk) {
  constexpr auto kExpectWrite = bytes::Array<0x02>();
  constexpr auto kExpectRead = bytes::Array<0x03>();
  Vector<std::byte, 4> register_addr{};
  register_addr.push_back(kExpectWrite[0]);

  auto transactions = MakeExpectedTransactionArray(
      {Transaction(OkStatus(),
                   Address{0x01},
                   kExpectWrite,
                   kExpectRead,
                   std::chrono::milliseconds(100))});
  MockInitiator i2c_initiator(transactions);

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cRead)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 0,
                .target_address = 0x01,
                .register_address = register_addr,
                .read_size = static_cast<uint32_t>(kExpectRead.size())});

  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), OkStatus());
  for (size_t i = 0; i < kExpectRead.size(); ++i) {
    EXPECT_EQ(kExpectRead[i], context.response().value[i]);
  }
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cReadMultiByteOk) {
  constexpr auto kExpectWrite = bytes::Array<0x02, 0x04, 0x06, 0x08>();
  constexpr auto kExpectRead = bytes::Array<0x03, 0x05, 0x07, 0x09>();
  Vector<std::byte, 4> register_addr{};
  std::copy(kExpectWrite.begin(),
            kExpectWrite.end(),
            std::back_inserter(register_addr));
  auto transactions = MakeExpectedTransactionArray(
      {Transaction(OkStatus(),
                   Address{0x01},
                   kExpectWrite,
                   kExpectRead,
                   std::chrono::milliseconds(100))});
  MockInitiator i2c_initiator(transactions);

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cRead)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 0,
                .target_address = 0x01,
                .register_address = register_addr,
                .read_size = static_cast<uint32_t>(kExpectRead.size())});

  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), OkStatus());
  for (size_t i = 0; i < kExpectRead.size(); ++i) {
    EXPECT_EQ(kExpectRead[i], context.response().value[i]);
  }
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cReadMaxByteOk) {
  constexpr auto kExpectWrite = bytes::Array<0x02, 0x04, 0x06, 0x08>();
  constexpr auto kExpectRead = bytes::Array<0x03, 0x05, 0x07, 0x09>();
  static_assert(sizeof(kExpectRead) <= pwpb::I2cReadResponse::kValueMaxSize);

  Vector<std::byte, 4> register_addr{};
  std::copy(kExpectWrite.begin(),
            kExpectWrite.end(),
            std::back_inserter(register_addr));
  auto transactions = MakeExpectedTransactionArray(
      {Transaction(OkStatus(),
                   Address{0x01},
                   kExpectWrite,
                   kExpectRead,
                   std::chrono::milliseconds(100))});
  MockInitiator i2c_initiator(transactions);

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cRead)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({
      .bus_index = 0,
      .target_address = 0x01,
      .register_address = register_addr,
      .read_size = sizeof(kExpectRead),
  });

  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), OkStatus());
  // EXPECT_EQ(kExpectRead, context.response().value);
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cReadMultiByteOutOfBounds) {
  pwpb::I2cReadResponse::Message response_message;
  constexpr auto kMaxReadSize = response_message.value.max_size();
  constexpr auto kRegisterAddr = bytes::Array<0x02, 0x04, 0x06, 0x08>();
  Vector<std::byte, 4> register_addr{};
  std::copy(kRegisterAddr.begin(),
            kRegisterAddr.end(),
            std::back_inserter(register_addr));
  MockInitiator i2c_initiator({});

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cRead)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 0,
                .target_address = 0x01,
                .register_address = register_addr,
                .read_size = kMaxReadSize + 1});

  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), Status::InvalidArgument());
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

TEST(I2cServiceTest, I2cReadInvalidBusIndex) {
  Vector<std::byte, 4> register_addr{};
  MockInitiator i2c_initiator({});

  PW_PWPB_TEST_METHOD_CONTEXT(I2cService, I2cRead)
  context{MakeSingletonSelector(&i2c_initiator)};

  context.call({.bus_index = 1,
                .target_address = 0x01,
                .register_address = register_addr,
                .read_size = 1});

  EXPECT_TRUE(context.done());
  EXPECT_EQ(context.status(), Status::InvalidArgument());
  EXPECT_EQ(i2c_initiator.Finalize(), OkStatus());
}

}  // namespace
}  // namespace pw::i2c
