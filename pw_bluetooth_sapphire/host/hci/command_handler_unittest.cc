// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_handler.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::hci {

namespace {

constexpr hci_spec::OpCode kOpCode(hci_spec::kInquiry);
constexpr uint8_t kTestEventParam = 3u;

template <bool DecodeSucceeds>
struct TestEvent {
  uint8_t test_param;
  static fitx::result<bt::Error<>, TestEvent> Decode(const EventPacket& packet) {
    if (!DecodeSucceeds) {
      return ToResult(HostError::kPacketMalformed).take_error();
    }

    return fitx::ok(TestEvent{.test_param = kTestEventParam});
  }

  static constexpr hci_spec::EventCode kEventCode = hci_spec::kInquiryCompleteEventCode;
};
using DecodableEvent = TestEvent<true>;
using UndecodableEvent = TestEvent<false>;

DynamicByteBuffer MakeTestEventPacket(
    hci_spec::StatusCode status = hci_spec::StatusCode::kSuccess) {
  return DynamicByteBuffer(StaticByteBuffer(DecodableEvent::kEventCode,
                                            0x01,  // parameters_total_size
                                            status));
}

template <bool DecodeSucceeds>
struct TestCommandCompleteEvent {
  uint8_t test_param;

  static fitx::result<bt::Error<>, TestCommandCompleteEvent> Decode(const EventPacket& packet) {
    if (!DecodeSucceeds) {
      return ToResult(HostError::kPacketMalformed).take_error();
    }

    return fitx::ok(TestCommandCompleteEvent{.test_param = kTestEventParam});
  }

  static constexpr hci_spec::EventCode kEventCode = hci_spec::kCommandCompleteEventCode;
};
using DecodableCommandCompleteEvent = TestCommandCompleteEvent<true>;
using UndecodableCommandCompleteEvent = TestCommandCompleteEvent<false>;

constexpr uint8_t kEncodedTestCommandParam = 2u;

template <typename CompleteEventT>
struct TestCommand {
  uint8_t test_param;

  std::unique_ptr<CommandPacket> Encode() {
    auto packet = CommandPacket::New(kOpCode, sizeof(test_param));
    auto* payload = packet->mutable_payload<decltype(test_param)>();
    *payload = kEncodedTestCommandParam;
    return packet;
  }

  static hci_spec::OpCode opcode() { return kOpCode; }

  using EventT = CompleteEventT;
};

const TestCommand<DecodableEvent> kTestCommandWithAsyncEvent{.test_param = 1u};
const TestCommand<DecodableCommandCompleteEvent> kTestCommandWithCommandCompleteEvent{.test_param =
                                                                                          1u};
const TestCommand<UndecodableCommandCompleteEvent> kTestCommandWithUndecodableCommandCompleteEvent{
    .test_param = 1u};

const auto kTestCommandPacket = StaticByteBuffer(LowerBits(kOpCode), UpperBits(kOpCode),
                                                 0x01,  // param length
                                                 kEncodedTestCommandParam);

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;
class CommandHandlerTest : public TestingBase {
 public:
  void SetUp() override {
    TestingBase::SetUp();
    StartTestDevice();
    handler_.emplace(cmd_channel()->AsWeakPtr());
  }

  CommandHandler& handler() { return handler_.value(); }

 private:
  std::optional<CommandHandler> handler_;
};

TEST_F(CommandHandlerTest, SuccessfulSendCommandWithSyncEvent) {
  const auto kEventPacket = testing::CommandCompletePacket(kOpCode, hci_spec::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<DecodableCommandCompleteEvent> event;
  handler().SendCommand(kTestCommandWithCommandCompleteEvent, [&event](auto result) {
    ASSERT_EQ(fitx::ok(), result);
    event = result.value();
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->test_param, kTestEventParam);
}

TEST_F(CommandHandlerTest, SendCommandReceiveFailEvent) {
  const auto kEventPacket =
      testing::CommandCompletePacket(kOpCode, hci_spec::StatusCode::kCommandDisallowed);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<hci::Error> error;
  handler().SendCommand(kTestCommandWithCommandCompleteEvent, [&error](auto result) {
    ASSERT_TRUE(result.is_error());
    error = std::move(result).error_value();
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_TRUE(error->is(hci_spec::StatusCode::kCommandDisallowed));
}

TEST_F(CommandHandlerTest, SendCommandWithSyncEventFailsToDecode) {
  const auto kEventPacket = testing::CommandCompletePacket(kOpCode, hci_spec::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kEventPacket);

  std::optional<hci::Error> error;
  handler().SendCommand(kTestCommandWithUndecodableCommandCompleteEvent, [&error](auto result) {
    ASSERT_TRUE(result.is_error());
    error = std::move(result).error_value();
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_TRUE(error->is(HostError::kPacketMalformed));
}

TEST_F(CommandHandlerTest, SuccessfulSendCommandWithAsyncEvent) {
  const auto kTestEventPacket = MakeTestEventPacket();
  const auto kStatusEventPacket =
      testing::CommandStatusPacket(kOpCode, hci_spec::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kStatusEventPacket, &kTestEventPacket);

  std::optional<DecodableEvent> event;
  size_t cb_count = 0;
  handler().SendCommand(kTestCommandWithAsyncEvent, [&event, &cb_count](auto result) {
    ASSERT_EQ(fitx::ok(), result);
    event = result.value();
    cb_count++;
  });

  RunLoopUntilIdle();
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
  RunLoopUntilIdle();
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
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 0u);
}

TEST_F(CommandHandlerTest, SendCommandFinishOnStatus) {
  const auto kStatusEventPacket =
      testing::CommandStatusPacket(kOpCode, hci_spec::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), kTestCommandPacket, &kStatusEventPacket);

  size_t cb_count = 0;
  handler().SendCommandFinishOnStatus(kTestCommandWithAsyncEvent, [&cb_count](auto result) {
    ASSERT_EQ(fitx::ok(), result);
    cb_count++;
  });

  RunLoopUntilIdle();
  ASSERT_EQ(cb_count, 1u);
}

}  // namespace
}  // namespace bt::hci
