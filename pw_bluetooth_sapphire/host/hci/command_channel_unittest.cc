// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"

#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt {
namespace hci {
namespace {

using bt::LowerBits;
using bt::UpperBits;
using bt::testing::CommandTransaction;
using EventCallbackResult = CommandChannel::EventCallbackResult;

using TestingBase = bt::testing::FakeControllerTest<bt::testing::TestController>;

// A reference counted object used to verify that HCI command completion and
// status callbacks are properly cleaned up after the end of a transaction.
class TestCallbackObject : public fxl::RefCountedThreadSafe<TestCallbackObject> {
 public:
  explicit TestCallbackObject(fit::closure deletion_callback)
      : deletion_cb_(std::move(deletion_callback)) {}

  virtual ~TestCallbackObject() { deletion_cb_(); }

 private:
  fit::closure deletion_cb_;
};

class HCI_CommandChannelTest : public TestingBase {
 public:
  HCI_CommandChannelTest() = default;
  ~HCI_CommandChannelTest() override = default;
};

std::unique_ptr<CommandPacket> MakeReadRemoteSupportedFeatures(uint16_t connection_handle) {
  auto packet = CommandPacket::New(kReadRemoteSupportedFeatures,
                                   sizeof(ReadRemoteSupportedFeaturesCommandParams));
  auto params = packet->mutable_view()->mutable_payload<ReadRemoteSupportedFeaturesCommandParams>();
  params->connection_handle = connection_handle;
  return packet;
}

TEST_F(HCI_CommandChannelTest, SingleRequestResponse) {
  // Set up expectations:
  // clang-format off
  // HCI_Reset
  auto req = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  // HCI_CommandComplete
  auto rsp = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      0x01,  // num_hci_command_packets (1 can be sent)
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      StatusCode::kHardwareFailure);
  // clang-format on
  test_device()->QueueCommandTransaction(CommandTransaction(req, {&rsp}));
  StartTestDevice();

  // Send a HCI_Reset command. We attach an instance of TestCallbackObject to
  // the callbacks to verify that it gets cleaned up as expected.
  bool test_obj_deleted = false;
  auto test_obj =
      fxl::MakeRefCounted<TestCallbackObject>([&test_obj_deleted] { test_obj_deleted = true; });

  auto reset = CommandPacket::New(kReset);
  CommandChannel::TransactionId id = cmd_channel()->SendCommand(
      std::move(reset), dispatcher(),
      [&id, test_obj](CommandChannel::TransactionId callback_id, const EventPacket& event) {
        EXPECT_EQ(id, callback_id);
        EXPECT_EQ(kCommandCompleteEventCode, event.event_code());
        EXPECT_EQ(4, event.view().header().parameter_total_size);
        EXPECT_EQ(1, event.view().payload<CommandCompleteEventParams>().num_hci_command_packets);
        EXPECT_EQ(kReset,
                  le16toh(event.view().payload<CommandCompleteEventParams>().command_opcode));
        EXPECT_EQ(StatusCode::kHardwareFailure, event.return_params<SimpleReturnParams>()->status);
      });

  test_obj = nullptr;
  EXPECT_FALSE(test_obj_deleted);
  RunLoopUntilIdle();

  // Make sure that the I/O thread is no longer holding on to |test_obj|.
  TearDown();

  EXPECT_TRUE(test_obj_deleted);
}

TEST_F(HCI_CommandChannelTest, SingleAsynchronousRequest) {
  // Set up expectations:
  // clang-format off
  // HCI_Inquiry (general, unlimited, 1s)
  auto req = CreateStaticByteBuffer(
      LowerBits(kInquiry), UpperBits(kInquiry),  // HCI_Inquiry opcode
      0x05,                                      // parameter_total_size
      0x33, 0x8B, 0x9E,                          // General Inquiry
      0x01,                                      // 1.28s
      0x00                                       // Unlimited responses
      );
  // HCI_CommandStatus
  auto rsp0 = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(kInquiry), UpperBits(kInquiry)  // HCI_Inquiry opcode
      );
  // HCI_InquiryComplete
  auto rsp1 = CreateStaticByteBuffer(
      kInquiryCompleteEventCode,
      0x01,  // parameter_total_size (1 byte payload)
      StatusCode::kSuccess);
  // clang-format on
  test_device()->QueueCommandTransaction(CommandTransaction(req, {&rsp0, &rsp1}));
  StartTestDevice();

  // Send HCI_Inquiry
  CommandChannel::TransactionId id;
  int cb_count = 0;
  auto cb = [&cb_count, &id](CommandChannel::TransactionId callback_id, const EventPacket& event) {
    cb_count++;
    EXPECT_EQ(callback_id, id);
    if (cb_count == 1) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
      const auto params = event.params<CommandStatusEventParams>();
      EXPECT_EQ(StatusCode::kSuccess, params.status);
      EXPECT_EQ(kInquiry, params.command_opcode);
    } else {
      EXPECT_EQ(kInquiryCompleteEventCode, event.event_code());
      EXPECT_TRUE(event.ToStatus());
    }
  };

  constexpr size_t kPayloadSize = sizeof(InquiryCommandParams);
  auto packet = CommandPacket::New(kInquiry, kPayloadSize);
  auto params = packet->mutable_payload<InquiryCommandParams>();
  params->lap = kGIAC;
  params->inquiry_length = 1;
  params->num_responses = 0;
  id = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb, kInquiryCompleteEventCode);
  RunLoopUntilIdle();
  EXPECT_EQ(2, cb_count);
}

TEST_F(HCI_CommandChannelTest, SingleRequestWithStatusResponse) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  // HCI_CommandStatus
  auto rsp = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  // clang-format on
  test_device()->QueueCommandTransaction(CommandTransaction(req, {&rsp}));
  StartTestDevice();

  // Send HCI_Reset
  CommandChannel::TransactionId id;
  auto complete_cb = [&id](CommandChannel::TransactionId callback_id, const EventPacket& event) {
    EXPECT_EQ(callback_id, id);
    EXPECT_EQ(kCommandStatusEventCode, event.event_code());
    EXPECT_EQ(StatusCode::kSuccess, event.params<CommandStatusEventParams>().status);
    EXPECT_EQ(1, event.view().payload<CommandStatusEventParams>().num_hci_command_packets);
    EXPECT_EQ(kReset, le16toh(event.params<CommandStatusEventParams>().command_opcode));
  };

  auto reset = CommandPacket::New(kReset);
  id = cmd_channel()->SendCommand(std::move(reset), dispatcher(), complete_cb,
                                  kCommandStatusEventCode);
  RunLoopUntilIdle();
}

// Tests:
//  - Only one HCI command sent until a status is received.
//  - Receiving a status update with a new number of packets available works.
TEST_F(HCI_CommandChannelTest, OneSentUntilStatus) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req1 = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp1 = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0x00,  // num_hci_command_packets (None can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  auto req2 = CreateStaticByteBuffer(
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp2 = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0x01,  // num_hci_command_packets (1 can be sent)
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel)  // HCI_InquiryCancel opcode
      );
  auto rsp_commandsavail = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      StatusCode::kSuccess, 0x01, // status, num_hci_command_packets (1 can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on
  test_device()->QueueCommandTransaction(CommandTransaction(req1, {&rsp1}));
  test_device()->QueueCommandTransaction(CommandTransaction(req2, {&rsp2}));
  StartTestDevice();

  CommandChannel::TransactionId reset_id, inquiry_id;
  size_t cb_event_count = 0u;
  size_t transaction_count = 0u;

  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  auto cb = [&cb_event_count](CommandChannel::TransactionId, const EventPacket& event) {
    EXPECT_EQ(kCommandCompleteEventCode, event.event_code());
    OpCode expected_opcode;
    if (cb_event_count == 0u) {
      expected_opcode = kReset;
    } else {
      expected_opcode = kInquiryCancel;
    }
    EXPECT_EQ(expected_opcode, le16toh(event.params<CommandCompleteEventParams>().command_opcode));
    cb_event_count++;
  };

  auto reset = CommandPacket::New(kReset);
  reset_id = cmd_channel()->SendCommand(std::move(reset), dispatcher(), cb);
  auto inquiry = CommandPacket::New(kInquiryCancel);
  inquiry_id = cmd_channel()->SendCommand(std::move(inquiry), dispatcher(), cb);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, transaction_count);
  EXPECT_EQ(1u, cb_event_count);

  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, transaction_count);
  EXPECT_EQ(2u, cb_event_count);
}

// Tests:
//  - Different opcodes can be sent concurrently
//  - Same opcodes are queued until a status opcode is sent.
TEST_F(HCI_CommandChannelTest, QueuedCommands) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req_reset = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_reset = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0xFF,  // num_hci_command_packets (255 can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  auto req_inqcancel = CreateStaticByteBuffer(
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_inqcancel = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0xFF,  // num_hci_command_packets (255 can be sent)
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel)  // HCI_Reset opcode
      );
  auto rsp_commandsavail = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      StatusCode::kSuccess, 0xFA, // status, num_hci_command_packets (250 can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on

  // We handle our own responses to make sure commands are queued.
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {}));
  test_device()->QueueCommandTransaction(CommandTransaction(req_inqcancel, {}));
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {&rsp_reset}));
  StartTestDevice();

  size_t transaction_count = 0u;
  size_t reset_count = 0u;
  size_t cancel_count = 0u;

  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  auto cb = [&reset_count, &cancel_count](CommandChannel::TransactionId id,
                                          const EventPacket& event) {
    EXPECT_EQ(kCommandCompleteEventCode, event.event_code());
    auto opcode = le16toh(event.params<CommandCompleteEventParams>().command_opcode);
    if (opcode == kReset) {
      reset_count++;
    } else if (opcode == kInquiryCancel) {
      cancel_count++;
    } else {
      EXPECT_TRUE(false) << "Unexpected opcode in command callback!";
    }
  };

  // CommandChannel only one can be sent - update num_hci_command_packets
  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  auto packet = CommandPacket::New(kReset);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb);
  packet = CommandPacket::New(kInquiryCancel);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb);
  packet = CommandPacket::New(kReset);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb);

  RunLoopUntilIdle();

  // Different opcodes can be sent without a reply
  EXPECT_EQ(2u, transaction_count);

  // Even if we get a response to one, the duplicate opcode is still queued.
  test_device()->SendCommandChannelPacket(rsp_inqcancel);
  RunLoopUntilIdle();

  EXPECT_EQ(2u, transaction_count);
  EXPECT_EQ(1u, cancel_count);
  EXPECT_EQ(0u, reset_count);

  // Once we get a reset back, the second can be sent (and replied to)
  test_device()->SendCommandChannelPacket(rsp_reset);
  RunLoopUntilIdle();

  EXPECT_EQ(3u, transaction_count);
  EXPECT_EQ(1u, cancel_count);
  EXPECT_EQ(2u, reset_count);
}

// Tests:
//  - Asynchronous commands are handled correctly (two callbacks, one for
//    status, one for complete)
//  - Asynchronous commands with the same event result are queued even if they
//    have different opcodes.
//  - Can't register an event handler when an asynchronous command is waiting.
TEST_F(HCI_CommandChannelTest, AsynchronousCommands) {
  constexpr EventCode kTestEventCode0 = 0xFE;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_resetstatus = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  auto req_inqcancel = CreateStaticByteBuffer(
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_inqstatus = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel)  // HCI_Reset opcode
      );
  auto rsp_bogocomplete = CreateStaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  // clang-format on

  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {&rsp_resetstatus}));
  test_device()->QueueCommandTransaction(CommandTransaction(req_inqcancel, {&rsp_inqstatus}));
  StartTestDevice();

  CommandChannel::TransactionId id1, id2;
  size_t cb_count = 0u;

  auto cb = [&id1, &id2, &cb_count, kTestEventCode0](CommandChannel::TransactionId callback_id,
                                                     const EventPacket& event) {
    if (cb_count < 2) {
      EXPECT_EQ(id1, callback_id);
    } else {
      EXPECT_EQ(id2, callback_id);
    }
    if ((cb_count % 2) == 0) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
      auto params = event.params<CommandStatusEventParams>();
      EXPECT_EQ(StatusCode::kSuccess, params.status);
    } else if ((cb_count % 2) == 1) {
      EXPECT_EQ(kTestEventCode0, event.event_code());
    }
    cb_count++;
  };

  auto packet = CommandPacket::New(kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb, kTestEventCode0);

  RunLoopUntilIdle();

  // Should have received the Status but not the result.
  EXPECT_EQ(1u, cb_count);

  // Setting another event up with different opcode will still queue the command
  // because we don't want to have two commands waiting on an event.
  packet = CommandPacket::New(kInquiryCancel);
  id2 = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb, kTestEventCode0);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, cb_count);

  // Sending the complete will release the queue and send the next command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunLoopUntilIdle();

  EXPECT_EQ(3u, cb_count);

  // Should not be able to register an event handler now, we're still waiting on
  // the asynchronous command.
  auto event_id0 = cmd_channel()->AddEventHandler(
      kTestEventCode0, [](const auto&) { return EventCallbackResult::kContinue; }, dispatcher());
  EXPECT_EQ(0u, event_id0);

  // Finish out the commands.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunLoopUntilIdle();

  EXPECT_EQ(4u, cb_count);
}

// Tests:
//  - Updating to say no commands can be sent works. (commands are queued)
//  - Can't add an event handler once a SendCommand() succeeds watiing on
//    the same event code. (even if they are queued)
TEST_F(HCI_CommandChannelTest, AsyncQueueWhenBlocked) {
  constexpr EventCode kTestEventCode0 = 0xF0;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_resetstatus = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  auto rsp_bogocomplete = CreateStaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  auto rsp_nocommandsavail = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      StatusCode::kSuccess, 0x00, // status, num_hci_command_packets (none can be sent)
      0x00, 0x00 // No associated opcode.
      );
  auto rsp_commandsavail = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      StatusCode::kSuccess, 0x01, // status, num_hci_command_packets (one can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on

  size_t transaction_count = 0u;

  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  test_device()->QueueCommandTransaction(
      CommandTransaction(req_reset, {&rsp_resetstatus, &rsp_bogocomplete}));
  StartTestDevice();

  test_device()->SendCommandChannelPacket(rsp_nocommandsavail);

  RunLoopUntilIdle();

  CommandChannel::TransactionId id;
  size_t cb_count = 0;
  auto cb = [&cb_count, &id, kTestEventCode0](CommandChannel::TransactionId callback_id,
                                              const EventPacket& event) {
    cb_count++;
    EXPECT_EQ(callback_id, id);
    if (cb_count == 1) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
      const auto params = event.params<CommandStatusEventParams>();
      EXPECT_EQ(StatusCode::kSuccess, params.status);
      EXPECT_EQ(kReset, params.command_opcode);
    } else {
      EXPECT_EQ(kTestEventCode0, event.event_code());
    }
  };

  auto packet = CommandPacket::New(kReset);
  id = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb, kTestEventCode0);

  RunLoopUntilIdle();

  ASSERT_NE(0u, id);
  ASSERT_EQ(0u, transaction_count);

  // This returns invalid because an async command is registered.
  auto invalid_id = cmd_channel()->AddEventHandler(
      kTestEventCode0, [](const auto&) { return EventCallbackResult::kContinue; }, dispatcher());

  RunLoopUntilIdle();

  ASSERT_EQ(0u, invalid_id);

  // Commands become available and the whole transaction finishes.
  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  RunLoopUntilIdle();

  ASSERT_EQ(1u, transaction_count);
  ASSERT_EQ(2u, cb_count);
}

// Tests:
//  - Events are routed to the event handler.
//  - Can't queue a command on the same event that is already in an event
//  handler.
TEST_F(HCI_CommandChannelTest, EventHandlerBasic) {
  constexpr EventCode kTestEventCode0 = 0xFE;
  constexpr EventCode kTestEventCode1 = 0xFF;
  auto cmd_status = CreateStaticByteBuffer(kCommandStatusEventCode, 0x04, 0x00, 0x01, 0x00, 0x00);
  auto cmd_complete = CreateStaticByteBuffer(kCommandCompleteEventCode, 0x03, 0x01, 0x00, 0x00);
  auto event0 = CreateStaticByteBuffer(kTestEventCode0, 0x00);
  auto event1 = CreateStaticByteBuffer(kTestEventCode1, 0x00);

  int event_count0 = 0;
  auto event_cb0 = [&event_count0, kTestEventCode0](const EventPacket& event) {
    event_count0++;
    EXPECT_EQ(kTestEventCode0, event.event_code());
    return EventCallbackResult::kContinue;
  };

  int event_count1 = 0;
  auto event_cb1 = [&event_count1, kTestEventCode0](const EventPacket& event) {
    event_count1++;
    EXPECT_EQ(kTestEventCode0, event.event_code());
    return EventCallbackResult::kContinue;
  };

  int event_count2 = 0;
  auto event_cb2 = [&event_count2, kTestEventCode1](const EventPacket& event) {
    event_count2++;
    EXPECT_EQ(kTestEventCode1, event.event_code());
    return EventCallbackResult::kContinue;
  };
  auto id0 = cmd_channel()->AddEventHandler(kTestEventCode0, event_cb0, dispatcher());
  EXPECT_NE(0u, id0);

  // Can register a handler for the same event code more than once.
  auto id1 = cmd_channel()->AddEventHandler(kTestEventCode0, event_cb1, dispatcher());
  EXPECT_NE(0u, id1);
  EXPECT_NE(id0, id1);

  // Add a handler for a different event code.
  auto id2 = cmd_channel()->AddEventHandler(kTestEventCode1, event_cb2, dispatcher());
  EXPECT_NE(0u, id2);

  auto reset = CommandPacket::New(kReset);
  auto transaction_id = cmd_channel()->SendCommand(
      std::move(reset), dispatcher(), [](auto, const auto&) {}, kTestEventCode0);

  EXPECT_EQ(0u, transaction_id);

  StartTestDevice();
  test_device()->SendCommandChannelPacket(cmd_status);
  test_device()->SendCommandChannelPacket(cmd_complete);
  test_device()->SendCommandChannelPacket(event1);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(cmd_complete);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(cmd_status);
  test_device()->SendCommandChannelPacket(event1);

  RunLoopUntilIdle();

  EXPECT_EQ(3, event_count0);
  EXPECT_EQ(3, event_count1);
  EXPECT_EQ(2, event_count2);

  event_count0 = 0;
  event_count1 = 0;
  event_count2 = 0;

  // Remove the first event handler.
  cmd_channel()->RemoveEventHandler(id0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event1);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event1);

  RunLoopUntilIdle();

  EXPECT_EQ(0, event_count0);
  EXPECT_EQ(7, event_count1);
  EXPECT_EQ(2, event_count2);

  event_count0 = 0;
  event_count1 = 0;
  event_count2 = 0;

  // Remove the second event handler.
  cmd_channel()->RemoveEventHandler(id1);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event1);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event1);
  test_device()->SendCommandChannelPacket(event1);

  RunLoopUntilIdle();

  EXPECT_EQ(0, event_count0);
  EXPECT_EQ(0, event_count1);
  EXPECT_EQ(3, event_count2);
}

// Tests:
//  - can't send a command that masks an event handler.
//  - can send a command without a callback.
TEST_F(HCI_CommandChannelTest, EventHandlerEventWhileTransactionPending) {
  // clang-format off
  // HCI_Reset
  auto req = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );

  auto req_complete = CreateStaticByteBuffer(
      kCommandCompleteEventCode,
      0x03,  // parameter_total_size (3 byte payload)
      0x01, // num_hci_command_packets (1 can be sent)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
      );
  // clang-format on

  constexpr EventCode kTestEventCode = 0xFF;
  auto event = CreateStaticByteBuffer(kTestEventCode, 0x01, 0x00);

  // We will send the HCI_Reset command with kTestEventCode as the completion
  // event. The event handler we register below should only get invoked once and
  // after the pending transaction completes.
  test_device()->QueueCommandTransaction(CommandTransaction(req, {&req_complete, &event, &event}));
  StartTestDevice();

  int event_count = 0;
  auto event_cb = [&event_count, kTestEventCode](const EventPacket& event) {
    event_count++;
    EXPECT_EQ(kTestEventCode, event.event_code());
    EXPECT_EQ(1u, event.view().header().parameter_total_size);
    EXPECT_EQ(1u, event.view().payload_size());
    return EventCallbackResult::kContinue;
  };

  cmd_channel()->AddEventHandler(kTestEventCode, event_cb, dispatcher());

  auto reset = CommandPacket::New(kReset);
  CommandChannel::TransactionId id =
      cmd_channel()->SendCommand(std::move(reset), dispatcher(), nullptr, kTestEventCode);
  EXPECT_EQ(0u, id);

  reset = CommandPacket::New(kReset);
  id = cmd_channel()->SendCommand(std::move(reset), dispatcher(), nullptr);
  EXPECT_NE(0u, id);

  RunLoopUntilIdle();

  EXPECT_EQ(2, event_count);
}

// Tests:
//  - Calling RemoveQueuedCommand on a synchronous command that has already been sent to the
//    controller returns false.
//  - The command still completes and notifies the callback.
TEST_F(HCI_CommandChannelTest, RemoveQueuedSyncCommandPendingStatus) {
  auto req_reset = CreateStaticByteBuffer(LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
                                          0x00  // parameter_total_size
  );
  auto rsp_reset = CreateStaticByteBuffer(kCommandCompleteEventCode,
                                          0x03,  // parameter_total_size (3 byte payload)
                                          0xFF,  // num_hci_command_packets (255 can be sent)
                                          LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
  );
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {}));
  StartTestDevice();

  int transaction_count = 0u;
  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  auto cmd = CommandPacket::New(kReset);
  int cmd_cb_count = 0;
  auto cmd_cb = [&cmd_cb_count](auto, auto&) { cmd_cb_count++; };
  auto cmd_id = cmd_channel()->SendCommand(std::move(cmd), dispatcher(), std::move(cmd_cb));
  EXPECT_NE(0u, cmd_id);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));
  test_device()->SendCommandChannelPacket(rsp_reset);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_EQ(1, cmd_cb_count);
}

// Tests:
//  - Remove a synchronous command that is queued up behind another command with the same opcode.
//  - The first command (after removal) does not receive the update event for the second command.
TEST_F(HCI_CommandChannelTest, RemoveQueuedQueuedSyncCommand) {
  using namespace std::placeholders;
  auto req_reset = CreateStaticByteBuffer(LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
                                          0x00  // parameter_total_size
  );
  auto rsp_reset = CreateStaticByteBuffer(kCommandCompleteEventCode,
                                          0x03,  // parameter_total_size (4 byte payload)
                                          0xFF,  // num_hci_command_packets (255 can be sent)
                                          LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
  );
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {}));
  StartTestDevice();

  int transaction_count = 0u;
  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  auto event_cb = [](CommandChannel::TransactionId id, const EventPacket& event, int* event_count) {
    EXPECT_EQ(kCommandCompleteEventCode, event.event_code());
    (*event_count)++;
  };

  // Send two reset commands so that the second one is queued up.
  auto reset = CommandPacket::New(kReset);
  int event_count0 = 0;
  auto id0 = cmd_channel()->SendCommand(std::move(reset), dispatcher(),
                                        std::bind(event_cb, _1, _2, &event_count0));
  EXPECT_NE(0u, id0);
  reset = CommandPacket::New(kReset);
  int event_count1 = 0;
  auto id1 = cmd_channel()->SendCommand(std::move(reset), dispatcher(),
                                        std::bind(event_cb, _1, _2, &event_count1));
  EXPECT_NE(0u, id1);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_TRUE(cmd_channel()->RemoveQueuedCommand(id1));

  RunLoopUntilIdle();

  EXPECT_EQ(0, event_count0);
  test_device()->SendCommandChannelPacket(rsp_reset);

  RunLoopUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The queued (then canceled) command should never have gotten an event.
  EXPECT_EQ(0, event_count1);
  // The sent command should have gotten one event (CommandComplete).
  EXPECT_EQ(1, event_count0);
}

// Read Remote Supported Features
const auto kReadRemoteSupportedFeaturesCmd = CreateStaticByteBuffer(
    LowerBits(kReadRemoteSupportedFeatures), UpperBits(kReadRemoteSupportedFeatures),
    0x02,       // parameter_total_size
    0x01, 0x00  // connection_handle
);

// Command Status for Read Remote Supported Features
const auto kReadRemoteSupportedFeaturesRsp =
    CreateStaticByteBuffer(kCommandStatusEventCode,
                           0x04,                  // parameter_total_size (4 byte payload)
                           StatusCode::kSuccess,  // status
                           0xFF,                  // num_hci_command_packets
                           LowerBits(kReadRemoteSupportedFeatures),
                           UpperBits(kReadRemoteSupportedFeatures)  // opcode
    );

// Read Remote Supported Features Complete
const auto kReadRemoteSupportedFeaturesComplete =
    CreateStaticByteBuffer(kReadRemoteSupportedFeaturesCompleteEventCode,
                           0x0B,                  // parameter_total_size (11 bytes)
                           StatusCode::kSuccess,  // status
                           0x01, 0x00,            // connection_handle
                           0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x80  // lmp_features
                           // Set: 3 slot packets, 5 slot packets, Encryption, Timing Accuracy,
                           // Role Switch, Hold Mode, Sniff Mode, LE Supported, Extended Features
    );

// Tests:
//  - Remove an asynchronous command that is queued up behind another command with the same opcode.
//  - The first command (after removal) does not receive the update event for the second command.
TEST_F(HCI_CommandChannelTest, RemoveQueuedQueuedAsyncCommand) {
  using namespace std::placeholders;
  test_device()->QueueCommandTransaction(CommandTransaction(kReadRemoteSupportedFeaturesCmd, {}));
  StartTestDevice();

  int transaction_count = 0u;
  test_device()->SetTransactionCallback([&transaction_count]() { transaction_count++; },
                                        dispatcher());

  auto event_cb = [](CommandChannel::TransactionId id, const EventPacket& event, int* event_count) {
    (*event_count)++;
  };

  // Send two read commands so that the second one is queued up.
  auto packet = MakeReadRemoteSupportedFeatures(0x0001);
  int event_count0 = 0;
  auto id0 = cmd_channel()->SendCommand(std::move(packet), dispatcher(),
                                        std::bind(event_cb, _1, _2, &event_count0),
                                        kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id0);
  packet = MakeReadRemoteSupportedFeatures(0x0001);
  int event_count1 = 0;
  auto id1 = cmd_channel()->SendCommand(std::move(packet), dispatcher(),
                                        std::bind(event_cb, _1, _2, &event_count1),
                                        kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id1);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_TRUE(cmd_channel()->RemoveQueuedCommand(id1));

  RunLoopUntilIdle();

  EXPECT_EQ(0, event_count0);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesRsp);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunLoopUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The queued (then canceled) command should never have gotten an event.
  EXPECT_EQ(0, event_count1);
  // The sent command should have gotten two events (Command Status, Read Remote
  // Supported Features Complete).
  EXPECT_EQ(2, event_count0);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has received both Command Status
//    and command completion events returns false and has no effect.
TEST_F(HCI_CommandChannelTest, RemoveQueuedCompletedAsyncCommand) {
  test_device()->QueueCommandTransaction(CommandTransaction(
      kReadRemoteSupportedFeaturesCmd,
      {&kReadRemoteSupportedFeaturesRsp, &kReadRemoteSupportedFeaturesComplete}));
  StartTestDevice();

  int transaction_count = 0;
  test_device()->SetTransactionCallback([&transaction_count] { transaction_count++; },
                                        dispatcher());

  int event_count = 0;
  auto event_cb = [&event_count](CommandChannel::TransactionId id, const EventPacket& event) {
    event_count++;
  };

  auto packet = MakeReadRemoteSupportedFeatures(0x0001);
  auto id = cmd_channel()->SendCommand(std::move(packet), dispatcher(), std::move(event_cb),
                                       kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id);

  RunLoopUntilIdle();

  EXPECT_EQ(2, event_count);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(id));

  RunLoopUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The sent command should have received CommandStatus and InquiryComplete.
  EXPECT_EQ(2, event_count);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has already been sent to the
//    controller returns false.
//  - The command still notifies the callback for update and completion events.
TEST_F(HCI_CommandChannelTest, RemoveQueuedAsyncCommandPendingUpdate) {
  test_device()->QueueCommandTransaction(CommandTransaction(kReadRemoteSupportedFeaturesCmd, {}));
  StartTestDevice();

  int transaction_count = 0;
  test_device()->SetTransactionCallback([&transaction_count] { transaction_count++; },
                                        dispatcher());

  CommandChannel::TransactionId cmd_id;
  int cmd_events = 0;
  auto cmd_cb = [&cmd_id, &cmd_events](CommandChannel::TransactionId id, const EventPacket& event) {
    EXPECT_EQ(cmd_id, id);
    if (cmd_events == 0) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
    }
    cmd_events++;
  };

  auto cmd_packet = MakeReadRemoteSupportedFeatures(0x0001);
  cmd_id = cmd_channel()->SendCommand(std::move(cmd_packet), dispatcher(), std::move(cmd_cb),
                                      kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, cmd_id);

  RunLoopUntilIdle();

  EXPECT_EQ(0, cmd_events);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));

  RunLoopUntilIdle();

  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesRsp);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  // The command should have gotten update and complete events.
  EXPECT_EQ(2, cmd_events);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has already been sent to the
//    controller and gotten Command Status returns false.
//  - The command still notifies the callback for completion event.
TEST_F(HCI_CommandChannelTest, RemoveQueuedAsyncCommandPendingCompletion) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kReadRemoteSupportedFeaturesCmd, {&kReadRemoteSupportedFeaturesRsp}));
  StartTestDevice();

  int transaction_count = 0;
  test_device()->SetTransactionCallback([&transaction_count] { transaction_count++; },
                                        dispatcher());

  CommandChannel::TransactionId cmd_id;
  int cmd_events = 0;
  auto cmd_cb = [&cmd_id, &cmd_events](CommandChannel::TransactionId id, const EventPacket& event) {
    EXPECT_EQ(cmd_id, id);
    if (cmd_events == 0) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
    }
    cmd_events++;
  };

  auto cmd_packet = MakeReadRemoteSupportedFeatures(0x0001);
  cmd_id = cmd_channel()->SendCommand(std::move(cmd_packet), dispatcher(), std::move(cmd_cb),
                                      kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, cmd_id);

  RunLoopUntilIdle();

  EXPECT_EQ(1, cmd_events);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));

  RunLoopUntilIdle();

  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count);
  // The command should have gotten update and complete events.
  EXPECT_EQ(2, cmd_events);
}

TEST_F(HCI_CommandChannelTest, LEMetaEventHandler) {
  constexpr EventCode kTestSubeventCode0 = 0xFE;
  constexpr EventCode kTestSubeventCode1 = 0xFF;
  auto le_meta_event_bytes0 =
      CreateStaticByteBuffer(hci::kLEMetaEventCode, 0x01, kTestSubeventCode0);
  auto le_meta_event_bytes1 =
      CreateStaticByteBuffer(hci::kLEMetaEventCode, 0x01, kTestSubeventCode1);

  int event_count0 = 0;
  auto event_cb0 = [&event_count0, kTestSubeventCode0](const EventPacket& event) {
    event_count0++;
    EXPECT_EQ(hci::kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode0, event.params<LEMetaEventParams>().subevent_code);
    return EventCallbackResult::kContinue;
  };

  int event_count1 = 0;
  auto event_cb1 = [&event_count1, kTestSubeventCode1](const EventPacket& event) {
    event_count1++;
    EXPECT_EQ(hci::kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode1, event.params<LEMetaEventParams>().subevent_code);
    return EventCallbackResult::kContinue;
  };

  auto id0 = cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode0, event_cb0, dispatcher());
  EXPECT_NE(0u, id0);

  // Can register a handler for the same event code more than once.
  auto id1 = cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode0, event_cb0, dispatcher());
  EXPECT_NE(0u, id1);
  EXPECT_NE(id0, id1);

  // Add a handler for a different event code.
  auto id2 = cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode1, event_cb1, dispatcher());
  EXPECT_NE(0u, id2);

  StartTestDevice();

  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  RunLoopUntilIdle();
  EXPECT_EQ(2, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  RunLoopUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(le_meta_event_bytes1);
  RunLoopUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(1, event_count1);

  // Remove the first event handler.
  cmd_channel()->RemoveEventHandler(id0);
  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  test_device()->SendCommandChannelPacket(le_meta_event_bytes1);
  RunLoopUntilIdle();
  EXPECT_EQ(5, event_count0);
  EXPECT_EQ(2, event_count1);
}

TEST_F(HCI_CommandChannelTest, EventHandlerIdsDontCollide) {
  // Add a LE Meta event handler and a event handler and make sure that IDs are
  // generated correctly across the two methods.
  EXPECT_EQ(1u, cmd_channel()->AddLEMetaEventHandler(
                    hci::kLEConnectionCompleteSubeventCode,
                    [](const auto&) { return EventCallbackResult::kContinue; }, dispatcher()));
  EXPECT_EQ(2u, cmd_channel()->AddEventHandler(
                    hci::kDisconnectionCompleteEventCode,
                    [](const auto&) { return EventCallbackResult::kContinue; }, dispatcher()));
}

// Tests:
//  - Can't register an event handler for CommandStatus or CommandComplete
TEST_F(HCI_CommandChannelTest, EventHandlerRestrictions) {
  auto id0 = cmd_channel()->AddEventHandler(
      hci::kCommandStatusEventCode, [](const auto&) { return EventCallbackResult::kContinue; },
      dispatcher());
  EXPECT_EQ(0u, id0);
  id0 = cmd_channel()->AddEventHandler(
      hci::kCommandCompleteEventCode, [](const auto&) { return EventCallbackResult::kContinue; },
      dispatcher());
  EXPECT_EQ(0u, id0);
}

// Tests that an asynchronous command with a completion event code does not
// remove an existing handler for colliding LE meta subevent code.
TEST_F(HCI_CommandChannelTest, AsyncEventHandlersAndLeMetaEventHandlersDoNotInterfere) {
  // Set up expectations for the asynchronous command and its corresponding
  // command status event.
  // clang-format off
  auto cmd = CreateStaticByteBuffer(
      LowerBits(kInquiry), UpperBits(kInquiry),  // HCI_Inquiry opcode
      0x00                                       // parameter_total_size
  );
  auto cmd_status = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(kInquiry), UpperBits(kInquiry)  // HCI_Inquiry opcode
  );
  // clang-format on

  test_device()->QueueCommandTransaction(CommandTransaction(cmd, {&cmd_status}));
  StartTestDevice();

  constexpr EventCode kTestEventCode = 0x01;

  // Add LE event handler for kTestEventCode
  int le_event_count = 0;
  auto le_event_cb = [&](const EventPacket& event) {
    EXPECT_EQ(kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestEventCode, event.params<LEMetaEventParams>().subevent_code);
    le_event_count++;
    return EventCallbackResult::kContinue;
  };
  cmd_channel()->AddLEMetaEventHandler(kLEConnectionCompleteSubeventCode, std::move(le_event_cb),
                                       dispatcher());

  // Initiate the async transaction with kTestEventCode as its completion code
  // (we use kInquiry as a dummy opcode).
  int async_cmd_cb_count = 0;
  auto async_cmd_cb = [&](auto id, const EventPacket& event) {
    if (async_cmd_cb_count == 0) {
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
    } else {
      EXPECT_EQ(kTestEventCode, event.event_code());
    }
    async_cmd_cb_count++;
  };
  auto packet = CommandPacket::New(kInquiry, 0);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), std::move(async_cmd_cb),
                             kTestEventCode);

  // clang-format off
  auto event_bytes = CreateStaticByteBuffer(
      kTestEventCode,
      0x01,  // parameter_total_size
      StatusCode::kSuccess);
  auto le_event_bytes = CreateStaticByteBuffer(
      kLEMetaEventCode,
      0x01,  // parameter_total_size
      kTestEventCode);
  // clang-format on

  // Send a spurious LE event before processing the Command Status event. This
  // should get routed to the correct event handler.
  test_device()->SendCommandChannelPacket(le_event_bytes);

  // Process the async command expectation.
  RunLoopUntilIdle();

  // End the asynchronous transaction. This should NOT unregister the LE event
  // handler.
  test_device()->SendCommandChannelPacket(event_bytes);

  // Send more LE events. These should get routed to the LE event handler.
  test_device()->SendCommandChannelPacket(le_event_bytes);
  test_device()->SendCommandChannelPacket(le_event_bytes);

  RunLoopUntilIdle();

  // Should have received 3 LE events.
  EXPECT_EQ(3, le_event_count);

  // The async command handler should have been called twice: once for Command
  // Status and once for the completion event.
  EXPECT_EQ(2, async_cmd_cb_count);
}

TEST_F(HCI_CommandChannelTest, TransportClosedCallback) {
  StartTestDevice();

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };
  transport()->SetTransportClosedCallback(closed_cb, dispatcher());

  async::PostTask(dispatcher(), [this] { test_device()->CloseCommandChannel(); });
  RunLoopUntilIdle();
  EXPECT_TRUE(closed_cb_called);
}

TEST_F(HCI_CommandChannelTest, CommandTimeout) {
  constexpr zx::duration kCommandTimeout = zx::sec(12);

  auto req_reset = CreateStaticByteBuffer(LowerBits(kReset), UpperBits(kReset),  // HCI_Reset opcode
                                          0x00  // parameter_total_size
  );

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {}));
  StartTestDevice();

  size_t cb_count = 0;
  CommandChannel::TransactionId id1, id2;
  auto cb = [&cb_count, &id1, &id2](CommandChannel::TransactionId callback_id,
                                    const EventPacket& event) {
    cb_count++;
    EXPECT_TRUE(callback_id == id1 || callback_id == id2);
    EXPECT_EQ(kCommandStatusEventCode, event.event_code());

    const auto params = event.params<CommandStatusEventParams>();
    EXPECT_EQ(StatusCode::kUnspecifiedError, params.status);
    EXPECT_EQ(kReset, params.command_opcode);
  };

  auto packet = CommandPacket::New(kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb);
  ASSERT_NE(0u, id1);

  packet = CommandPacket::New(kReset);
  id2 = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb);
  ASSERT_NE(0u, id2);

  // Run the loop until the command timeout task gets scheduled.
  RunLoopUntilIdle();
  ASSERT_EQ(0u, cb_count);

  RunLoopFor(kCommandTimeout);
  ;

  EXPECT_EQ(2u, cb_count);
}

// Tests:
//  - Asynchronous commands should be able to schedule another asynchronous
//    command in their callback.
TEST_F(HCI_CommandChannelTest, AsynchronousCommandChaining) {
  constexpr size_t kExpectedCallbacksPerCommand = 2;
  constexpr EventCode kTestEventCode0 = 0xFE;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = CreateStaticByteBuffer(
      LowerBits(kReset), UpperBits(kReset), // HCI_Reset opcode
      0x00                                  // parameter_total_size (no payload)
      );
  auto rsp_resetstatus = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,                        // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0xFA,  // status, num_hci_command_packets (250)
      LowerBits(kReset), UpperBits(kReset)  // HCI_Reset opcode
  );
  auto req_inqcancel = CreateStaticByteBuffer(
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel), // HCI_InquiryCancel
      0x00                        // parameter_total_size (no payload)
  );
  auto rsp_inqstatus = CreateStaticByteBuffer(
      kCommandStatusEventCode,
      0x04,                        // parameter_total_size (4 byte payload)
      StatusCode::kSuccess, 0xFA,  // status, num_hci_command_packets (250)
      LowerBits(kInquiryCancel), UpperBits(kInquiryCancel) // HCI_InquiryCanacel
  );
  auto rsp_bogocomplete = CreateStaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  // clang-format on

  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {&rsp_resetstatus}));
  test_device()->QueueCommandTransaction(CommandTransaction(req_reset, {&rsp_resetstatus}));
  StartTestDevice();

  CommandChannel::TransactionId id1, id2;
  CommandChannel::CommandCallback cb;
  size_t cb_count = 0u;

  cb = [&cb, cmd_channel = cmd_channel(), dispatcher = dispatcher(), &id1, &id2, &cb_count,
        kTestEventCode0](CommandChannel::TransactionId callback_id, const EventPacket& event) {
    if (cb_count < kExpectedCallbacksPerCommand) {
      EXPECT_EQ(id1, callback_id);
    } else {
      EXPECT_EQ(id2, callback_id);
    }
    if ((cb_count % 2) == 0) {
      // First event from each command - CommandStatus
      EXPECT_EQ(kCommandStatusEventCode, event.event_code());
      auto params = event.params<CommandStatusEventParams>();
      EXPECT_EQ(StatusCode::kSuccess, params.status);
    } else {
      // Second event from each command - completion event
      EXPECT_EQ(kTestEventCode0, event.event_code());
      if (cb_count < 2) {
        // Add the second command when the first one completes.
        auto packet = CommandPacket::New(kReset);
        id2 = cmd_channel->SendCommand(std::move(packet), dispatcher, cb.share(), kTestEventCode0);
      }
    }
    cb_count++;
  };

  auto packet = CommandPacket::New(kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), dispatcher(), cb.share(), kTestEventCode0);

  RunLoopUntilIdle();

  // Should have received the Status but not the result.
  EXPECT_EQ(1u, cb_count);

  // Sending the complete will finish the command and add the next command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunLoopUntilIdle();

  EXPECT_EQ(3u, cb_count);

  // Finish out the command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunLoopUntilIdle();

  EXPECT_EQ(4u, cb_count);
}

// Tests:
//  - Commands that are exclusive of other commands cannot run together, and
//    instead wait until the exclusive commands finish.
//  - Exclusive Commands in the queue still get started in order
//  - Commands that aren't exclusive run as normal even when an exclusive one is
//    waiting.
TEST_F(HCI_CommandChannelTest, ExclusiveCommands) {
  constexpr EventCode kExclOneCompleteEvent = 0xFE;
  constexpr EventCode kExclTwoCompleteEvent = 0xFD;
  constexpr OpCode kExclusiveOne = DefineOpCode(0x01, 0x01);
  constexpr OpCode kExclusiveTwo = DefineOpCode(0x01, 0x02);
  constexpr OpCode kNonExclusive = DefineOpCode(0x01, 0x03);

  // Set up expectations
  //  - kExclusiveOne can't run at the same time as kExclusiveTwo, and
  //  vice-versa.
  //  - kExclusiveOne finishes with kExclOneCompleteEvent
  //  - kExclusiveTwo finishes with kExclTwoCompleteEvent
  //  - kNonExclusive can run whenever it wants.
  //  - For testing, we omit the payloads of all commands.
  auto excl_one_cmd = CreateStaticByteBuffer(
      LowerBits(kExclusiveOne), UpperBits(kExclusiveOne), 0x00  // (no payload)
  );
  auto rsp_excl_one_status =
      CreateStaticByteBuffer(kCommandStatusEventCode,
                             0x04,                        // parameter_total_size (4 byte payload)
                             StatusCode::kSuccess, 0xFA,  // status, num_hci_command_packets (250)
                             LowerBits(kExclusiveOne),
                             UpperBits(kExclusiveOne)  // HCI opcode
      );
  auto rsp_one_complete =
      CreateStaticByteBuffer(kExclOneCompleteEvent, 0x00  // parameter_total_size (no payload)
      );

  auto excl_two_cmd = CreateStaticByteBuffer(
      LowerBits(kExclusiveTwo), UpperBits(kExclusiveTwo), 0x00  // (no payload)
  );
  auto rsp_excl_two_status =
      CreateStaticByteBuffer(kCommandStatusEventCode,
                             0x04,                        // parameter_total_size (4 byte payload)
                             StatusCode::kSuccess, 0xFA,  // status, num_hci_command_packets (250)
                             LowerBits(kExclusiveTwo),
                             UpperBits(kExclusiveTwo)  // HCI opcode
      );
  auto rsp_two_complete =
      CreateStaticByteBuffer(kExclTwoCompleteEvent, 0x00  // parameter_total_size (no payload)
      );

  auto nonexclusive_cmd =
      CreateStaticByteBuffer(LowerBits(kNonExclusive), UpperBits(kNonExclusive),  // HCI opcode
                             0x00  // parameter_total_size (no payload)
      );
  auto nonexclusive_complete =
      CreateStaticByteBuffer(kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             0xFA,  // num_hci_command_packets (250)
                             LowerBits(kNonExclusive), UpperBits(kNonExclusive),  // HCI opcode
                             StatusCode::kSuccess  // Command succeeded
      );

  test_device()->QueueCommandTransaction(CommandTransaction(excl_one_cmd, {&rsp_excl_one_status}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(nonexclusive_cmd, {&nonexclusive_complete}));
  test_device()->QueueCommandTransaction(CommandTransaction(excl_two_cmd, {&rsp_excl_two_status}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(nonexclusive_cmd, {&nonexclusive_complete}));
  test_device()->QueueCommandTransaction(CommandTransaction(excl_one_cmd, {&rsp_excl_one_status}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(nonexclusive_cmd, {&nonexclusive_complete}));

  StartTestDevice();

  CommandChannel::TransactionId id1, id2, id3;
  CommandChannel::CommandCallback exclusive_cb;
  size_t exclusive_cb_count = 0u;

  size_t nonexclusive_cb_count = 0;
  CommandChannel::CommandCallback nonexclusive_cb =
      [&nonexclusive_cb_count](auto callback_id, const EventPacket& event) {
        EXPECT_EQ(kCommandCompleteEventCode, event.event_code());
        nonexclusive_cb_count++;
      };

  exclusive_cb = [&exclusive_cb, &nonexclusive_cb, cmd_channel = cmd_channel(),
                  dispatcher = dispatcher(), &id1, &id2, &id3, &exclusive_cb_count,
                  kExclOneCompleteEvent, kExclTwoCompleteEvent](
                     CommandChannel::TransactionId callback_id, const EventPacket& event) {
    // Expected event -> Action in response
    // 0. Status for kExclusiveOne -> Send a kExclusiveTwo
    // 1. Complete for kExclusiveOne -> Send Another kExclusiveOne and
    // kNonExclusive
    // 2. Status for kExclusiveTwo -> Nothing
    // 3. Complete for kExclusiveTwo -> Nothing
    // 4. Status for kExclusiveOne -> Nothing
    // 5. Complete for kExclusiveOne -> Nothing
    switch (exclusive_cb_count) {
      case 0: {
        // Status for kExclusiveOne -> Send kExclusiveTwo (queued)
        EXPECT_EQ(id1, callback_id);
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        auto params = event.params<CommandStatusEventParams>();
        EXPECT_EQ(StatusCode::kSuccess, params.status);
        auto packet = CommandPacket::New(kExclusiveTwo);
        id2 = cmd_channel->SendExclusiveCommand(std::move(packet), dispatcher, exclusive_cb.share(),
                                                kExclTwoCompleteEvent, {kExclusiveOne});
        std::cout << "queued Exclusive Two: " << id2 << std::endl;
        break;
      }
      case 1: {
        // Complete for kExclusiveOne -> Resend kExclusiveOne
        EXPECT_EQ(id1, callback_id);
        EXPECT_EQ(kExclOneCompleteEvent, event.event_code());
        // Add the second command when the first one completes.
        auto packet = CommandPacket::New(kExclusiveOne);
        id3 = cmd_channel->SendExclusiveCommand(std::move(packet), dispatcher, exclusive_cb.share(),
                                                kExclOneCompleteEvent, {kExclusiveTwo});
        std::cout << "queued Second Exclusive One: " << id3 << std::endl;
        packet = CommandPacket::New(kNonExclusive);
        cmd_channel->SendCommand(std::move(packet), dispatcher, nonexclusive_cb.share());

        break;
      }
      case 2: {  // Status for kExclusiveTwo
        EXPECT_EQ(id2, callback_id);
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        auto params = event.params<CommandStatusEventParams>();
        EXPECT_EQ(StatusCode::kSuccess, params.status);
        break;
      }
      case 3: {  // Complete for kExclusiveTwo
        EXPECT_EQ(id2, callback_id);
        EXPECT_EQ(kExclTwoCompleteEvent, event.event_code());
        break;
      }
      case 4: {  // Status for Second kExclusiveOne
        EXPECT_EQ(id3, callback_id);
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        auto params = event.params<CommandStatusEventParams>();
        EXPECT_EQ(StatusCode::kSuccess, params.status);
        break;
      }
      case 5: {  // Complete for Second kExclusiveOne
        EXPECT_EQ(id3, callback_id);
        EXPECT_EQ(kExclOneCompleteEvent, event.event_code());
        break;
      }
      default: {
        ASSERT_TRUE(false);  // Should never be called more than 6 times.
        break;
      }
    }
    exclusive_cb_count++;
  };

  auto packet = CommandPacket::New(kExclusiveOne);
  id1 = cmd_channel()->SendExclusiveCommand(std::move(packet), dispatcher(), exclusive_cb.share(),
                                            kExclOneCompleteEvent, {kExclusiveTwo});
  packet = CommandPacket::New(kNonExclusive);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), nonexclusive_cb.share());

  RunLoopUntilIdle();

  // Should have received the Status but not the result.
  EXPECT_EQ(1u, exclusive_cb_count);
  // But the WriteLocalName should be fine.
  EXPECT_EQ(1u, nonexclusive_cb_count);

  // Sending the complete will finish the command and add the next command.
  test_device()->SendCommandChannelPacket(rsp_one_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(3u, exclusive_cb_count);
  EXPECT_EQ(2u, nonexclusive_cb_count);

  // Finish out the ExclusiveTwo
  test_device()->SendCommandChannelPacket(rsp_two_complete);
  packet = CommandPacket::New(kNonExclusive);
  cmd_channel()->SendCommand(std::move(packet), dispatcher(), nonexclusive_cb.share());
  RunLoopUntilIdle();

  EXPECT_EQ(5u, exclusive_cb_count);
  EXPECT_EQ(3u, nonexclusive_cb_count);

  // Finish the second kExclusiveOne
  test_device()->SendCommandChannelPacket(rsp_one_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(6u, exclusive_cb_count);
  EXPECT_EQ(3u, nonexclusive_cb_count);
}

TEST_F(HCI_CommandChannelTest, EventHandlerResults) {
  constexpr EventCode kTestEventCode0 = 0xFE;
  auto cmd_status = CreateStaticByteBuffer(kCommandStatusEventCode, 0x04, 0x00, 0x01, 0x00, 0x00);
  auto cmd_complete = CreateStaticByteBuffer(kCommandCompleteEventCode, 0x03, 0x01, 0x00, 0x00);
  auto event0 = CreateStaticByteBuffer(kTestEventCode0, 0x00);

  int event_count = 0;
  auto event_cb = [&event_count, kTestEventCode0](const EventPacket& event) {
    event_count++;
    EXPECT_EQ(kTestEventCode0, event.event_code());
    if (event_count == 1) {
      return EventCallbackResult::kContinue;
    } else {
      return EventCallbackResult::kRemove;
    }
  };

  auto id0 = cmd_channel()->AddEventHandler(kTestEventCode0, event_cb, dispatcher());
  EXPECT_NE(0u, id0);

  auto reset = CommandPacket::New(kReset);
  auto transaction_id = cmd_channel()->SendCommand(
      std::move(reset), dispatcher(), [](auto, const auto&) {}, kTestEventCode0);

  EXPECT_EQ(0u, transaction_id);

  StartTestDevice();
  test_device()->SendCommandChannelPacket(cmd_status);
  test_device()->SendCommandChannelPacket(cmd_complete);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);

  // Without waiting for the event loop to process the handler thrice,
  // the handler will not be removed before the fourth event is received,
  // and the handler will be placed into the dispatcher and called a fourth time.
  RunLoopUntilIdle();

  EXPECT_EQ(3, event_count);

  test_device()->SendCommandChannelPacket(event0);

  RunLoopUntilIdle();

  EXPECT_EQ(3, event_count);
}

TEST_F(HCI_CommandChannelTest, SendCommandWithLEMetaEventSubeventRsp) {
  constexpr OpCode kOpCode = kLEReadRemoteFeatures;
  constexpr EventCode kSubeventCode = kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd = StaticByteBuffer(LowerBits(kOpCode), UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);

  auto cmd_status_event = StaticByteBuffer(kCommandStatusEventCode,
                                           // parameter total size (4 byte payload)
                                           0x04,
                                           // status, num_hci_command_packets (250)
                                           StatusCode::kSuccess, 0xFA,
                                           // HCI opcode
                                           LowerBits(kOpCode), UpperBits(kOpCode));
  auto cmd_complete_subevent = StaticByteBuffer(kLEMetaEventCode,
                                                0x01,  // parameter total size (1 byte payload)
                                                kSubeventCode);

  test_device()->QueueCommandTransaction(CommandTransaction(cmd, {&cmd_status_event}));
  StartTestDevice();

  auto cmd_packet = CommandPacket::New(kOpCode);

  size_t event_count = 0;
  auto event_cb = [&event_count](auto, const EventPacket& event) {
    switch (event_count) {
      case 0: {
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count++;
  };
  auto id = cmd_channel()->SendLeAsyncCommand(std::move(cmd_packet), dispatcher(),
                                              std::move(event_cb), kSubeventCode);
  EXPECT_NE(0u, id);

  RunLoopUntilIdle();
  EXPECT_EQ(1u, event_count);

  // Handler should be removed when subevent received.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunLoopUntilIdle();
  EXPECT_EQ(2u, event_count);

  // This seconod complete event should be ignored because the handler should have been removed.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunLoopUntilIdle();
  EXPECT_EQ(2u, event_count);
}

TEST_F(
    HCI_CommandChannelTest,
    SendingLECommandAfterAddingLEMetaEventHandlerFailsForSameSubeventCodeAndSucceedsForDifferentSubeventCode) {
  constexpr EventCode kSubeventCode = kLEReadRemoteFeaturesCompleteSubeventCode;
  constexpr OpCode kOpCode = kLEReadRemoteFeatures;  // LE Read Remote Features

  StartTestDevice();

  EXPECT_NE(0u, cmd_channel()->AddLEMetaEventHandler(
                    kSubeventCode, [](const auto&) { return EventCallbackResult::kContinue; },
                    dispatcher()));
  EXPECT_EQ(
      0u, cmd_channel()->SendLeAsyncCommand(
              CommandPacket::New(kOpCode), dispatcher(), [](auto, const auto&) {}, kSubeventCode));

  auto cmd = StaticByteBuffer(LowerBits(kOpCode), UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);
  test_device()->QueueCommandTransaction(CommandTransaction(std::move(cmd), {}));
  EXPECT_NE(0u, cmd_channel()->SendLeAsyncCommand(
                    CommandPacket::New(kOpCode), dispatcher(), [](auto, const auto&) {},
                    kSubeventCode + 1));
  RunLoopUntilIdle();
}

TEST_F(HCI_CommandChannelTest, SendingSecondLECommandWithSameSubeventShouldWaitForFirstToComplete) {
  // Commands have different op codes but same subevent code so that second command is not
  // blocked because of matching op codes (which would not test LE command handling).
  constexpr OpCode kOpCode0 = kLEReadRemoteFeatures;
  constexpr OpCode kOpCode1 = kLEReadBufferSize;
  constexpr EventCode kSubeventCode = kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd0 = StaticByteBuffer(LowerBits(kOpCode0), UpperBits(kOpCode0),
                               // parameter total size (0 byte payload)
                               0x00);
  auto cmd0_status_event = StaticByteBuffer(kCommandStatusEventCode,
                                            // parameter total size (4 byte payload)
                                            0x04,
                                            // status, num_hci_command_packets (250)
                                            StatusCode::kSuccess, 0xFA,
                                            // HCI opcode
                                            LowerBits(kOpCode0), UpperBits(kOpCode0));
  auto cmd1 = StaticByteBuffer(LowerBits(kOpCode1), UpperBits(kOpCode1),
                               // parameter total size (0 byte payload)
                               0x00);
  auto cmd1_status_event = StaticByteBuffer(kCommandStatusEventCode,
                                            // parameter total size (4 byte payload)
                                            0x04,
                                            // status, num_hci_command_packets (250)
                                            StatusCode::kSuccess, 0xFA,
                                            // HCI opcode
                                            LowerBits(kOpCode1), UpperBits(kOpCode1));

  auto cmd_complete_subevent = StaticByteBuffer(kLEMetaEventCode,
                                                0x01,  // parameter total size (1 byte payload)
                                                kSubeventCode);

  test_device()->QueueCommandTransaction(CommandTransaction(cmd0, {&cmd0_status_event}));
  StartTestDevice();

  size_t event_count_0 = 0;
  auto event_cb_0 = [&event_count_0](auto, const EventPacket& event) {
    switch (event_count_0) {
      case 0: {
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count_0++;
  };
  auto id_0 = cmd_channel()->SendLeAsyncCommand(CommandPacket::New(kOpCode0), dispatcher(),
                                                std::move(event_cb_0), kSubeventCode);
  EXPECT_NE(0u, id_0);

  RunLoopUntilIdle();
  EXPECT_EQ(1u, event_count_0);

  size_t event_count_1 = 0;
  auto event_cb_1 = [&event_count_1](auto, const EventPacket& event) {
    switch (event_count_1) {
      case 0: {
        EXPECT_EQ(kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count_1++;
  };
  // Command should be queued and not sent until after first complete event received.
  auto id_1 = cmd_channel()->SendLeAsyncCommand(CommandPacket::New(kOpCode1), dispatcher(),
                                                std::move(event_cb_1), kSubeventCode);
  EXPECT_NE(0u, id_1);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, event_count_1);

  // When first command complete event is received, second command should be sent.
  test_device()->QueueCommandTransaction(CommandTransaction(cmd1, {&cmd1_status_event}));
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunLoopUntilIdle();
  EXPECT_EQ(2u, event_count_0);
  EXPECT_EQ(1u, event_count_1);

  // Second complete event should be received by second command event handler only.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunLoopUntilIdle();
  EXPECT_EQ(2u, event_count_0);
  EXPECT_EQ(2u, event_count_1);
}

TEST_F(
    HCI_CommandChannelTest,
    RegisteringLEMetaEventHandlerWhileLECommandPendingFailsForSameSubeventAndSucceedsForDifferentSubevent) {
  constexpr OpCode kOpCode = kLEReadRemoteFeatures;
  constexpr EventCode kSubeventCode = kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd = StaticByteBuffer(LowerBits(kOpCode), UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);

  auto cmd_status_event = StaticByteBuffer(kCommandStatusEventCode,
                                           // parameter total size (4 byte payload)
                                           0x04,
                                           // status, num_hci_command_packets (250)
                                           StatusCode::kSuccess, 0xFA,
                                           // HCI opcode
                                           LowerBits(kOpCode), UpperBits(kOpCode));

  test_device()->QueueCommandTransaction(CommandTransaction(cmd, {&cmd_status_event}));
  StartTestDevice();

  size_t event_count = 0;
  auto event_cb = [&event_count](auto, const EventPacket& event) {
    EXPECT_EQ(kCommandStatusEventCode, event.event_code());
    event_count++;
  };
  auto id = cmd_channel()->SendLeAsyncCommand(CommandPacket::New(kOpCode), dispatcher(),
                                              std::move(event_cb), kSubeventCode);
  EXPECT_NE(0u, id);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, event_count);

  // Async LE command for subevent is already pending, so registering event handler should fail by
  // returning 0.
  id = cmd_channel()->AddLEMetaEventHandler(
      kSubeventCode, [](auto&) { return EventCallbackResult::kContinue; }, dispatcher());
  EXPECT_EQ(0u, id);

  // Registering event handler for different subevent code should succeed.
  id = cmd_channel()->AddLEMetaEventHandler(
      kSubeventCode + 1, [](auto&) { return EventCallbackResult::kContinue; }, dispatcher());
  EXPECT_NE(0u, id);
}

}  // namespace
}  // namespace hci
}  // namespace bt
