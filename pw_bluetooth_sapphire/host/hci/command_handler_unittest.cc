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

#include "pw_bluetooth_sapphire/internal/host/hci/command_handler.h"

#include <pw_bluetooth/hci_common.emb.h>
#include <pw_bluetooth/hci_test.emb.h>

#include "pw_bluetooth_sapphire/internal/host/common/error.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::hci {

namespace {

constexpr hci_spec::OpCode kOpCode(hci_spec::kInquiry);
constexpr uint8_t kTestEventParam = 3u;

template <bool DecodeSucceeds>
struct TestEvent {
  uint8_t test_param;
  static fit::result<bt::Error<>, TestEvent> Decode(const EventPacket& packet) {
    if (!DecodeSucceeds) {
      return fit::error(bt::Error(HostError::kPacketMalformed));
    }

    return fit::ok(TestEvent{.test_param = kTestEventParam});
  }

  static constexpr hci_spec::EventCode kEventCode =
      hci_spec::kInquiryCompleteEventCode;
};
using DecodableEvent = TestEvent<true>;
using UndecodableEvent = TestEvent<false>;

DynamicByteBuffer MakeTestEventPacket(
    pw::bluetooth::emboss::StatusCode status =
        pw::bluetooth::emboss::StatusCode::SUCCESS) {
  return DynamicByteBuffer(StaticByteBuffer(DecodableEvent::kEventCode,
                                            0x01,  // parameters_total_size
                                            status));
}

template <bool DecodeSucceeds>
struct TestCommandCompleteEvent {
  uint8_t test_param;

  static fit::result<bt::Error<>, TestCommandCompleteEvent> Decode(
      const EventPacket& packet) {
    if (!DecodeSucceeds) {
      return fit::error(bt::Error(HostError::kPacketMalformed));
    }

    return fit::ok(TestCommandCompleteEvent{.test_param = kTestEventParam});
  }

  static constexpr hci_spec::EventCode kEventCode =
      hci_spec::kCommandCompleteEventCode;
};
using DecodableCommandCompleteEvent = TestCommandCompleteEvent<true>;
using UndecodableCommandCompleteEvent = TestCommandCompleteEvent<false>;

constexpr uint8_t kEncodedTestCommandParam = 2u;

template <typename CompleteEventT>
struct TestCommand {
  uint8_t test_param;

  EmbossCommandPacket Encode() {
    auto packet = EmbossCommandPacket::New<
        pw::bluetooth::emboss::TestCommandPacketWriter>(kOpCode);
    packet.view_t().payload().Write(kEncodedTestCommandParam);
    return packet;
  }

  static hci_spec::OpCode opcode() { return kOpCode; }

  using EventT = CompleteEventT;
};

const TestCommand<DecodableEvent> kTestCommandWithAsyncEvent{.test_param = 1u};
const TestCommand<DecodableCommandCompleteEvent>
    kTestCommandWithCommandCompleteEvent{.test_param = 1u};
const TestCommand<UndecodableCommandCompleteEvent>
    kTestCommandWithUndecodableCommandCompleteEvent{.test_param = 1u};

const StaticByteBuffer kTestCommandPacket(LowerBits(kOpCode),
                                          UpperBits(kOpCode),
                                          0x01,  // param length
                                          kEncodedTestCommandParam);

using TestingBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;
class CommandHandlerTest : public TestingBase {
 public:
  void SetUp() override {
    TestingBase::SetUp();

    handler_.emplace(cmd_channel()->AsWeakPtr());
  }

  CommandHandler& handler() { return handler_.value(); }

 private:
  std::optional<CommandHandler> handler_;
};

TEST_F(CommandHandlerTest, SuccessfulSendCommandWithSyncEvent) {
  const auto kEventPacket = bt::testing::CommandCompletePacket(
      kOpCode, pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<DecodableCommandCompleteEvent> event;
  handler().SendCommand(kTestCommandWithCommandCompleteEvent,
                        [&event](auto result) {
                          ASSERT_EQ(fit::ok(), result);
                          event = result.value();
                        });

  RunUntilIdle();
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->test_param, kTestEventParam);
}

TEST_F(CommandHandlerTest, SendCommandReceiveFailEvent) {
  const auto kEventPacket = bt::testing::CommandCompletePacket(
      kOpCode, pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<hci::Error> error;
  handler().SendCommand(kTestCommandWithCommandCompleteEvent,
                        [&error](auto result) {
                          ASSERT_TRUE(result.is_error());
                          error = std::move(result).error_value();
                        });

  RunUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_TRUE(error->is(pw::bluetooth::emboss::StatusCode::COMMAND_DISALLOWED));
}

TEST_F(CommandHandlerTest, SendCommandWithSyncEventFailsToDecode) {
  const auto kEventPacket = bt::testing::CommandCompletePacket(
      kOpCode, pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<hci::Error> error;
  handler().SendCommand(kTestCommandWithUndecodableCommandCompleteEvent,
                        [&error](auto result) {
                          ASSERT_TRUE(result.is_error());
                          error = std::move(result).error_value();
                        });

  RunUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_TRUE(error->is(HostError::kPacketMalformed));
}

TEST_F(CommandHandlerTest, SuccessfulSendCommandWithAsyncEvent) {
  const auto kTestEventPacket = MakeTestEventPacket();
  const auto kStatusEventPacket = bt::testing::CommandStatusPacket(
      kOpCode, pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        kTestCommandPacket,
                        &kStatusEventPacket,
                        &kTestEventPacket);

  std::optional<DecodableEvent> event;
  size_t cb_count = 0;
  handler().SendCommand(kTestCommandWithAsyncEvent,
                        [&event, &cb_count](auto result) {
                          ASSERT_EQ(fit::ok(), result);
                          event = result.value();
                          cb_count++;
                        });

  RunUntilIdle();
  ASSERT_EQ(cb_count, 1u);
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->test_param, kTestEventParam);
}

TEST_F(CommandHandlerTest, AddEventHandlerSuccess) {
  std::optional<DecodableEvent> event;
  size_t cb_count = 0;
  handler().AddEventHandler<DecodableEvent>([&event, &cb_count](auto cb_event) {
    cb_count++;
    event = cb_event;
    return CommandChannel::EventCallbackResult::kContinue;
  });
  test_device()->SendCommandChannelPacket(MakeTestEventPacket());
  test_device()->SendCommandChannelPacket(MakeTestEventPacket());
  RunUntilIdle();
  EXPECT_EQ(cb_count, 2u);
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->test_param, kTestEventParam);
}

TEST_F(CommandHandlerTest, AddEventHandlerDecodeError) {
  size_t cb_count = 0;
  handler().AddEventHandler<UndecodableEvent>([&cb_count](auto cb_event) {
    cb_count++;
    return CommandChannel::EventCallbackResult::kContinue;
  });
  test_device()->SendCommandChannelPacket(MakeTestEventPacket());
  test_device()->SendCommandChannelPacket(MakeTestEventPacket());
  RunUntilIdle();
  EXPECT_EQ(cb_count, 0u);
}

TEST_F(CommandHandlerTest, SendCommandFinishOnStatus) {
  const auto kStatusEventPacket = bt::testing::CommandStatusPacket(
      kOpCode, pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kStatusEventPacket);

  size_t cb_count = 0;
  handler().SendCommandFinishOnStatus(kTestCommandWithAsyncEvent,
                                      [&cb_count](auto result) {
                                        ASSERT_EQ(fit::ok(), result);
                                        cb_count++;
                                      });

  RunUntilIdle();
  ASSERT_EQ(cb_count, 1u);
}

}  // namespace
}  // namespace bt::hci
