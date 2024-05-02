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

#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

#include <pw_bluetooth/hci_android.emb.h>
#include <pw_bluetooth/hci_commands.emb.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"

namespace bt::hci {
namespace {

using namespace inspect::testing;

using bt::LowerBits;
using bt::UpperBits;
using EventCallbackResult = CommandChannel::EventCallbackResult;

constexpr pw::chrono::SystemClock::duration kCommandTimeout =
    std::chrono::seconds(12);

using TestingBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

// A reference counted object used to verify that HCI command completion and
// status callbacks are properly cleaned up after the end of a transaction.
class TestCallbackObject {
 public:
  explicit TestCallbackObject(fit::closure deletion_callback)
      : deletion_cb_(std::move(deletion_callback)) {}

  virtual ~TestCallbackObject() { deletion_cb_(); }

 private:
  fit::closure deletion_cb_;
};

class CommandChannelTest : public TestingBase {
 public:
  CommandChannelTest() = default;
  ~CommandChannelTest() override = default;

  pw::async::HeapDispatcher heap_dispatcher() { return heap_dispatcher_; }

  inspect::Inspector inspector_;

 private:
  pw::async::HeapDispatcher heap_dispatcher_{dispatcher()};
};

EmbossCommandPacket MakeReadRemoteSupportedFeatures(
    uint16_t connection_handle) {
  auto packet = EmbossCommandPacket::New<
      pw::bluetooth::emboss::ReadRemoteSupportedFeaturesCommandWriter>(
      hci_spec::kReadRemoteSupportedFeatures);
  packet.view_t().connection_handle().Write(connection_handle);
  return packet;
}

TEST_F(CommandChannelTest, SingleRequestResponse) {
  // Set up expectations:
  // clang-format off
  // HCI_Reset
  StaticByteBuffer req(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  // HCI_CommandComplete
 StaticByteBuffer rsp(
      hci_spec::kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      0x01,  // num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE);
  // clang-format on
  EXPECT_CMD_PACKET_OUT(test_device(), req, &rsp);

  // Send a HCI_Reset command. We attach an instance of TestCallbackObject to
  // the callbacks to verify that it gets cleaned up as expected.
  bool test_obj_deleted = false;
  auto test_obj = std::make_shared<TestCallbackObject>(
      [&test_obj_deleted] { test_obj_deleted = true; });

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id = cmd_channel()->SendCommand(
      std::move(reset),
      [&id, test_obj](CommandChannel::TransactionId callback_id,
                      const EventPacket& event) {
        EXPECT_EQ(id, callback_id);
        EXPECT_EQ(hci_spec::kCommandCompleteEventCode, event.event_code());
        EXPECT_EQ(4, event.view().header().parameter_total_size);
        EXPECT_EQ(1,
                  event.view()
                      .payload<hci_spec::CommandCompleteEventParams>()
                      .num_hci_command_packets);
        EXPECT_EQ(hci_spec::kReset,
                  le16toh(event.view()
                              .payload<hci_spec::CommandCompleteEventParams>()
                              .command_opcode));
        EXPECT_EQ(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE,
                  event.return_params<hci_spec::SimpleReturnParams>()->status);
      });

  test_obj = nullptr;
  EXPECT_FALSE(test_obj_deleted);
  RunUntilIdle();

  // Make sure that the I/O thread is no longer holding on to |test_obj|.
  TearDown();

  EXPECT_TRUE(test_obj_deleted);
}

TEST_F(CommandChannelTest, SingleAsynchronousRequest) {
  // Set up expectations:
  // clang-format off
  // HCI_Inquiry (general, unlimited, 1s)
 StaticByteBuffer req(
      LowerBits(hci_spec::kInquiry), UpperBits(hci_spec::kInquiry),  // HCI_Inquiry opcode
      0x05,                                      // parameter_total_size
      0x33, 0x8B, 0x9E,                          // General Inquiry
      0x01,                                      // 1.28s
      0x00                                       // Unlimited responses
      );
  // HCI_CommandStatus
  auto rsp0 = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kInquiry), UpperBits(hci_spec::kInquiry)  // HCI_Inquiry opcode
      );
  // HCI_InquiryComplete
  auto rsp1 = StaticByteBuffer(
      hci_spec::kInquiryCompleteEventCode,
      0x01,  // parameter_total_size (1 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  // clang-format on
  EXPECT_CMD_PACKET_OUT(test_device(), req, &rsp0, &rsp1);

  // Send HCI_Inquiry
  CommandChannel::TransactionId id;
  int cb_count = 0;
  auto cb = [&cb_count, &id](CommandChannel::TransactionId callback_id,
                             const EventPacket& event) {
    cb_count++;
    EXPECT_EQ(callback_id, id);
    if (cb_count == 1) {
      ASSERT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
      const auto params = event.params<hci_spec::CommandStatusEventParams>();
      EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
      EXPECT_EQ(hci_spec::kInquiry, params.command_opcode);
    } else {
      EXPECT_EQ(hci_spec::kInquiryCompleteEventCode, event.event_code());
      EXPECT_EQ(fit::ok(), event.ToResult());
    }
  };

  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::InquiryCommandWriter>(hci_spec::kInquiry);
  auto view = packet.view_t();
  view.lap().Write(pw::bluetooth::emboss::InquiryAccessCode::GIAC);
  view.inquiry_length().Write(1);
  view.num_responses().Write(0);

  id = cmd_channel()->SendCommand(
      std::move(packet), cb, hci_spec::kInquiryCompleteEventCode);
  RunUntilIdle();
  EXPECT_EQ(2, cb_count);
}

TEST_F(CommandChannelTest, SingleRequestWithStatusResponse) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  // HCI_CommandStatus
  auto rsp = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  // clang-format on
  EXPECT_CMD_PACKET_OUT(test_device(), req, &rsp);

  // Send HCI_Reset
  CommandChannel::TransactionId id;
  auto complete_cb = [&id](CommandChannel::TransactionId callback_id,
                           const EventPacket& event) {
    EXPECT_EQ(callback_id, id);
    EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
    EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS,
              event.params<hci_spec::CommandStatusEventParams>().status);
    EXPECT_EQ(1,
              event.view()
                  .payload<hci_spec::CommandStatusEventParams>()
                  .num_hci_command_packets);
    EXPECT_EQ(
        hci_spec::kReset,
        le16toh(
            event.params<hci_spec::CommandStatusEventParams>().command_opcode));
  };

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id = cmd_channel()->SendCommand(
      std::move(reset), complete_cb, hci_spec::kCommandStatusEventCode);
  RunUntilIdle();
}

// Tests:
//  - Only one HCI command sent until a status is received.
//  - Receiving a status update with a new number of packets available works.
TEST_F(CommandChannelTest, OneSentUntilStatus) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req1 = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp1 = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0x00,  // num_hci_command_packets (None can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  auto req2 = StaticByteBuffer(
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp2 = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0x01,  // num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel)  // HCI_InquiryCancel opcode
      );
  auto rsp_commandsavail = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x01, // status, num_hci_command_packets (1 can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on
  EXPECT_CMD_PACKET_OUT(test_device(), req1, &rsp1);
  EXPECT_CMD_PACKET_OUT(test_device(), req2, &rsp2);

  size_t cb_event_count = 0u;
  size_t transaction_count = 0u;

  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  auto cb = [&cb_event_count](CommandChannel::TransactionId,
                              const EventPacket& event) {
    EXPECT_EQ(hci_spec::kCommandCompleteEventCode, event.event_code());
    hci_spec::OpCode expected_opcode;
    if (cb_event_count == 0u) {
      expected_opcode = hci_spec::kReset;
    } else {
      expected_opcode = hci_spec::kInquiryCancel;
    }
    EXPECT_EQ(expected_opcode,
              le16toh(event.params<hci_spec::CommandCompleteEventParams>()
                          .command_opcode));
    cb_event_count++;
  };

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  [[maybe_unused]] auto reset_id =
      cmd_channel()->SendCommand(std::move(reset), cb);
  auto inquiry = CommandPacket::New(hci_spec::kInquiryCancel);
  [[maybe_unused]] auto inquiry_id =
      cmd_channel()->SendCommand(std::move(inquiry), cb);

  RunUntilIdle();

  EXPECT_EQ(1u, transaction_count);
  EXPECT_EQ(1u, cb_event_count);

  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  RunUntilIdle();

  EXPECT_EQ(2u, transaction_count);
  EXPECT_EQ(2u, cb_event_count);
}

// Tests:
//  - Different opcodes can be sent concurrently
//  - Same opcodes are queued until a status opcode is sent.
TEST_F(CommandChannelTest, QueuedCommands) {
  // Set up expectations
  // clang-format off
  // HCI_Reset for the sake of testing
  auto req_reset = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_reset = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0xFF,  // num_hci_command_packets (255 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  auto req_inqcancel = StaticByteBuffer(
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_inqcancel = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x03,  // parameter_total_size (4 byte payload)
      0xFF,  // num_hci_command_packets (255 can be sent)
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel)  // HCI_Reset opcode
      );
  auto rsp_commandsavail = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA, // status, num_hci_command_packets (250 can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on

  // We handle our own responses to make sure commands are queued.
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, );
  EXPECT_CMD_PACKET_OUT(test_device(), req_inqcancel, );
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, &rsp_reset);

  size_t transaction_count = 0u;
  size_t reset_count = 0u;
  size_t cancel_count = 0u;

  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  auto cb = [&reset_count, &cancel_count](CommandChannel::TransactionId id,
                                          const EventPacket& event) {
    EXPECT_EQ(hci_spec::kCommandCompleteEventCode, event.event_code());
    auto opcode = le16toh(
        event.params<hci_spec::CommandCompleteEventParams>().command_opcode);
    if (opcode == hci_spec::kReset) {
      reset_count++;
    } else if (opcode == hci_spec::kInquiryCancel) {
      cancel_count++;
    } else {
      EXPECT_TRUE(false) << "Unexpected opcode in command callback!";
    }
  };

  // CommandChannel only one can be sent - update num_hci_command_packets
  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  cmd_channel()->SendCommand(std::move(reset), cb);
  auto inquiry_cancel = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::InquiryCancelCommandWriter>(
      hci_spec::kInquiryCancel);
  cmd_channel()->SendCommand(std::move(inquiry_cancel), cb);
  reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  cmd_channel()->SendCommand(std::move(reset), cb);

  RunUntilIdle();

  // Different opcodes can be sent without a reply
  EXPECT_EQ(2u, transaction_count);

  // Even if we get a response to one, the duplicate opcode is still queued.
  test_device()->SendCommandChannelPacket(rsp_inqcancel);
  RunUntilIdle();

  EXPECT_EQ(2u, transaction_count);
  EXPECT_EQ(1u, cancel_count);
  EXPECT_EQ(0u, reset_count);

  // Once we get a reset back, the second can be sent (and replied to)
  test_device()->SendCommandChannelPacket(rsp_reset);
  RunUntilIdle();

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
TEST_F(CommandChannelTest, AsynchronousCommands) {
  constexpr hci_spec::EventCode kTestEventCode0 = 0xFE;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_resetstatus = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  auto req_inqcancel = StaticByteBuffer(
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel),  // HCI_InquiryCancel opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_inqstatus = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel)  // HCI_Reset opcode
      );
  auto rsp_bogocomplete = StaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, &rsp_resetstatus);
  EXPECT_CMD_PACKET_OUT(test_device(), req_inqcancel, &rsp_inqstatus);

  CommandChannel::TransactionId id1, id2;
  size_t cb_count = 0u;

  auto cb = [&id1, &id2, &cb_count, kTestEventCode0](
                CommandChannel::TransactionId callback_id,
                const EventPacket& event) {
    if (cb_count < 2) {
      EXPECT_EQ(id1, callback_id);
    } else {
      EXPECT_EQ(id2, callback_id);
    }
    if ((cb_count % 2) == 0) {
      EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
      auto params = event.params<hci_spec::CommandStatusEventParams>();
      EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
    } else if ((cb_count % 2) == 1) {
      EXPECT_EQ(kTestEventCode0, event.event_code());
    }
    cb_count++;
  };

  hci::EmbossCommandPacket packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), cb, kTestEventCode0);

  RunUntilIdle();

  // Should have received the Status but not the result.
  EXPECT_EQ(1u, cb_count);

  // Setting another event up with different opcode will still queue the command
  // because we don't want to have two commands waiting on an event.
  packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::InquiryCancelCommandWriter>(
      hci_spec::kInquiryCancel);
  id2 = cmd_channel()->SendCommand(std::move(packet), cb, kTestEventCode0);
  RunUntilIdle();

  EXPECT_EQ(1u, cb_count);

  // Sending the complete will release the queue and send the next command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunUntilIdle();

  EXPECT_EQ(3u, cb_count);

  // Should not be able to register an event handler now, we're still waiting on
  // the asynchronous command.
  auto event_id0 = cmd_channel()->AddEventHandler(
      kTestEventCode0,
      [](const EventPacket&) { return EventCallbackResult::kContinue; });
  EXPECT_EQ(0u, event_id0);

  // Finish out the commands.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunUntilIdle();

  EXPECT_EQ(4u, cb_count);
}

// Tests:
//  - Updating to say no commands can be sent works. (commands are queued)
//  - Can't add an event handler once a SendCommand() succeeds watiing on
//    the same event code. (even if they are queued)
TEST_F(CommandChannelTest, AsyncQueueWhenBlocked) {
  constexpr hci_spec::EventCode kTestEventCode0 = 0xF0;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );
  auto rsp_resetstatus = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA, // status, num_hci_command_packets (250 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  auto rsp_bogocomplete = StaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  auto rsp_nocommandsavail = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x00, // status, num_hci_command_packets (none can be sent)
      0x00, 0x00 // No associated opcode.
      );
  auto rsp_commandsavail = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x01, // status, num_hci_command_packets (one can be sent)
      0x00, 0x00 // No associated opcode.
      );
  // clang-format on

  size_t transaction_count = 0u;

  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  EXPECT_CMD_PACKET_OUT(
      test_device(), req_reset, &rsp_resetstatus, &rsp_bogocomplete);

  test_device()->SendCommandChannelPacket(rsp_nocommandsavail);

  RunUntilIdle();

  CommandChannel::TransactionId id;
  size_t cb_count = 0;
  auto cb = [&cb_count, &id, kTestEventCode0](
                CommandChannel::TransactionId callback_id,
                const EventPacket& event) {
    cb_count++;
    EXPECT_EQ(callback_id, id);
    if (cb_count == 1) {
      ASSERT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
      const auto params = event.params<hci_spec::CommandStatusEventParams>();
      EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
      EXPECT_EQ(hci_spec::kReset, params.command_opcode);
    } else {
      EXPECT_EQ(kTestEventCode0, event.event_code());
    }
  };

  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id = cmd_channel()->SendCommand(std::move(packet), cb, kTestEventCode0);

  RunUntilIdle();

  ASSERT_NE(0u, id);
  ASSERT_EQ(0u, transaction_count);

  // This returns invalid because an async command is registered.
  auto invalid_id = cmd_channel()->AddEventHandler(
      kTestEventCode0,
      [](const EventPacket&) { return EventCallbackResult::kContinue; });

  RunUntilIdle();

  ASSERT_EQ(0u, invalid_id);

  // Commands become available and the whole transaction finishes.
  test_device()->SendCommandChannelPacket(rsp_commandsavail);

  RunUntilIdle();

  ASSERT_EQ(1u, transaction_count);
  ASSERT_EQ(2u, cb_count);
}

// Tests:
//  - Events are routed to the event handler.
//  - Can't queue a command on the same event that is already in an event
//  handler.
TEST_F(CommandChannelTest, EventHandlerBasic) {
  constexpr hci_spec::EventCode kTestEventCode0 = 0xFD;
  constexpr hci_spec::EventCode kTestEventCode1 = 0xFE;
  StaticByteBuffer cmd_status(
      hci_spec::kCommandStatusEventCode, 0x04, 0x00, 0x01, 0x00, 0x00);
  auto cmd_complete = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode, 0x03, 0x01, 0x00, 0x00);
  auto event0 = StaticByteBuffer(kTestEventCode0, 0x00);
  auto event1 = StaticByteBuffer(kTestEventCode1, 0x00);

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
  auto id0 = cmd_channel()->AddEventHandler(kTestEventCode0, event_cb0);
  EXPECT_NE(0u, id0);

  // Can register a handler for the same event code more than once.
  auto id1 = cmd_channel()->AddEventHandler(kTestEventCode0, event_cb1);
  EXPECT_NE(0u, id1);
  EXPECT_NE(id0, id1);

  // Add a handler for a different event code.
  auto id2 = cmd_channel()->AddEventHandler(kTestEventCode1, event_cb2);
  EXPECT_NE(0u, id2);

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  auto transaction_id = cmd_channel()->SendCommand(
      std::move(reset), [](auto, const auto&) {}, kTestEventCode0);

  EXPECT_EQ(0u, transaction_id);

  test_device()->SendCommandChannelPacket(cmd_status);
  test_device()->SendCommandChannelPacket(cmd_complete);
  test_device()->SendCommandChannelPacket(event1);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(cmd_complete);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(cmd_status);
  test_device()->SendCommandChannelPacket(event1);

  RunUntilIdle();

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

  RunUntilIdle();

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

  RunUntilIdle();

  EXPECT_EQ(0, event_count0);
  EXPECT_EQ(0, event_count1);
  EXPECT_EQ(3, event_count2);
}

// Tests:
//  - can't send a command that masks an event handler.
//  - can send a command without a callback.
TEST_F(CommandChannelTest, EventHandlerEventWhileTransactionPending) {
  // clang-format off
  // HCI_Reset
  auto req = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset),  // HCI_Reset opcode
      0x00                                   // parameter_total_size
      );

  auto req_complete = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x03,  // parameter_total_size (3 byte payload)
      0x01, // num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  // clang-format on

  constexpr hci_spec::EventCode kTestEventCode = 0xFE;
  auto event = StaticByteBuffer(kTestEventCode, 0x01, 0x00);

  // We will send the HCI_Reset command with kTestEventCode as the completion
  // event. The event handler we register below should only get invoked once and
  // after the pending transaction completes.
  EXPECT_CMD_PACKET_OUT(test_device(), req, &req_complete, &event, &event);

  int event_count = 0;
  auto event_cb = [&event_count, kTestEventCode](const EventPacket& event) {
    event_count++;
    EXPECT_EQ(kTestEventCode, event.event_code());
    EXPECT_EQ(1u, event.view().header().parameter_total_size);
    EXPECT_EQ(1u, event.view().payload_size());
    return EventCallbackResult::kContinue;
  };

  cmd_channel()->AddEventHandler(kTestEventCode, event_cb);

  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id =
      cmd_channel()->SendCommand(std::move(reset), nullptr, kTestEventCode);
  EXPECT_EQ(0u, id);

  reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id = cmd_channel()->SendCommand(std::move(reset), nullptr);
  EXPECT_NE(0u, id);

  RunUntilIdle();

  EXPECT_EQ(2, event_count);
}

// Tests:
//  - Calling RemoveQueuedCommand on a synchronous command that has already been
//  sent to the
//    controller returns false.
//  - The command still completes and notifies the callback.
TEST_F(CommandChannelTest, RemoveQueuedSyncCommandPendingStatus) {
  auto req_reset =
      StaticByteBuffer(LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                       0x00                          // parameter_total_size
      );
  auto rsp_reset =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x03,  // parameter_total_size (3 byte payload)
                       0xFF,  // num_hci_command_packets (255 can be sent)
                       LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, );

  int transaction_count = 0u;
  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  auto cmd =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  int cmd_cb_count = 0;
  auto cmd_cb = [&cmd_cb_count](auto, auto&) { cmd_cb_count++; };
  auto cmd_id = cmd_channel()->SendCommand(std::move(cmd), std::move(cmd_cb));
  EXPECT_NE(0u, cmd_id);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));
  test_device()->SendCommandChannelPacket(rsp_reset);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_EQ(1, cmd_cb_count);
}

// Tests:
//  - Remove a synchronous command that is queued up behind another command with
//  the same opcode.
//  - The first command (after removal) does not receive the update event for
//  the second command.
TEST_F(CommandChannelTest, RemoveQueuedQueuedSyncCommand) {
  using namespace std::placeholders;
  auto req_reset =
      StaticByteBuffer(LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                       0x00                          // parameter_total_size
      );
  auto rsp_reset =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x03,  // parameter_total_size (4 byte payload)
                       0xFF,  // num_hci_command_packets (255 can be sent)
                       LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset)  // HCI_Reset opcode
      );
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, );

  int transaction_count = 0u;
  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  auto event_cb = [](CommandChannel::TransactionId id,
                     const EventPacket& event,
                     int* event_count) {
    EXPECT_EQ(hci_spec::kCommandCompleteEventCode, event.event_code());
    (*event_count)++;
  };

  // Send two reset commands so that the second one is queued up.
  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  int event_count0 = 0;
  auto id0 = cmd_channel()->SendCommand(
      std::move(reset), std::bind(event_cb, _1, _2, &event_count0));
  EXPECT_NE(0u, id0);
  reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  int event_count1 = 0;
  auto id1 = cmd_channel()->SendCommand(
      std::move(reset), std::bind(event_cb, _1, _2, &event_count1));
  EXPECT_NE(0u, id1);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_TRUE(cmd_channel()->RemoveQueuedCommand(id1));

  RunUntilIdle();

  EXPECT_EQ(0, event_count0);
  test_device()->SendCommandChannelPacket(rsp_reset);

  RunUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The queued (then canceled) command should never have gotten an event.
  EXPECT_EQ(0, event_count1);
  // The sent command should have gotten one event (CommandComplete).
  EXPECT_EQ(1, event_count0);
}

// Read Remote Supported Features
const StaticByteBuffer kReadRemoteSupportedFeaturesCmd(
    LowerBits(hci_spec::kReadRemoteSupportedFeatures),
    UpperBits(hci_spec::kReadRemoteSupportedFeatures),
    0x02,  // parameter_total_size
    0x01,
    0x00  // connection_handle
);

// Command Status for Read Remote Supported Features
const auto kReadRemoteSupportedFeaturesRsp = StaticByteBuffer(
    hci_spec::kCommandStatusEventCode,
    0x04,  // parameter_total_size (4 byte payload)
    pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
    0xFF,                                        // num_hci_command_packets
    LowerBits(hci_spec::kReadRemoteSupportedFeatures),
    UpperBits(hci_spec::kReadRemoteSupportedFeatures)  // opcode
);

// Read Remote Supported Features Complete
const auto kReadRemoteSupportedFeaturesComplete = StaticByteBuffer(
    hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
    0x0B,  // parameter_total_size (11 bytes)
    pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
    0x01,
    0x00,  // connection_handle
    0xFF,
    0x00,
    0x00,
    0x00,
    0x02,
    0x00,
    0x00,
    0x80  // lmp_features
          // Set: 3 slot packets, 5 slot packets, Encryption, Timing Accuracy,
          // Role Switch, Hold Mode, Sniff Mode, LE Supported, Extended Features
);

// Tests:
//  - Remove an asynchronous command that is queued up behind another command
//  with the same opcode.
//  - The first command (after removal) does not receive the update event for
//  the second command.
TEST_F(CommandChannelTest, RemoveQueuedQueuedAsyncCommand) {
  using namespace std::placeholders;
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeaturesCmd, );

  int transaction_count = 0u;
  test_device()->SetTransactionCallback(
      [&transaction_count]() { transaction_count++; });

  auto event_cb = [](CommandChannel::TransactionId id,
                     const EventPacket& event,
                     int* event_count) { (*event_count)++; };

  // Send two read commands so that the second one is queued up.
  auto packet = MakeReadRemoteSupportedFeatures(0x0001);
  int event_count0 = 0;
  auto id0 = cmd_channel()->SendCommand(
      std::move(packet),
      std::bind(event_cb, _1, _2, &event_count0),
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id0);
  packet = MakeReadRemoteSupportedFeatures(0x0001);
  int event_count1 = 0;
  auto id1 = cmd_channel()->SendCommand(
      std::move(packet),
      std::bind(event_cb, _1, _2, &event_count1),
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id1);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  EXPECT_TRUE(cmd_channel()->RemoveQueuedCommand(id1));

  RunUntilIdle();

  EXPECT_EQ(0, event_count0);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesRsp);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The queued (then canceled) command should never have gotten an event.
  EXPECT_EQ(0, event_count1);
  // The sent command should have gotten two events (Command Status, Read Remote
  // Supported Features Complete).
  EXPECT_EQ(2, event_count0);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has received
//  both Command Status
//    and command completion events returns false and has no effect.
TEST_F(CommandChannelTest, RemoveQueuedCompletedAsyncCommand) {
  EXPECT_CMD_PACKET_OUT(test_device(),
                        kReadRemoteSupportedFeaturesCmd,
                        &kReadRemoteSupportedFeaturesRsp,
                        &kReadRemoteSupportedFeaturesComplete);

  int transaction_count = 0;
  test_device()->SetTransactionCallback(
      [&transaction_count] { transaction_count++; });

  int event_count = 0;
  auto event_cb = [&event_count](CommandChannel::TransactionId id,
                                 const EventPacket& event) { event_count++; };

  auto packet = MakeReadRemoteSupportedFeatures(0x0001);
  auto id = cmd_channel()->SendCommand(
      std::move(packet),
      std::move(event_cb),
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, id);

  RunUntilIdle();

  EXPECT_EQ(2, event_count);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(id));

  RunUntilIdle();

  // Only one command should have been sent.
  EXPECT_EQ(1, transaction_count);
  // The sent command should have received CommandStatus and InquiryComplete.
  EXPECT_EQ(2, event_count);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has already
//  been sent to the
//    controller returns false.
//  - The command still notifies the callback for update and completion events.
TEST_F(CommandChannelTest, RemoveQueuedAsyncCommandPendingUpdate) {
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeaturesCmd, );

  int transaction_count = 0;
  test_device()->SetTransactionCallback(
      [&transaction_count] { transaction_count++; });

  CommandChannel::TransactionId cmd_id;
  int cmd_events = 0;
  auto cmd_cb = [&cmd_id, &cmd_events](CommandChannel::TransactionId id,
                                       const EventPacket& event) {
    EXPECT_EQ(cmd_id, id);
    if (cmd_events == 0) {
      EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
    }
    cmd_events++;
  };

  auto cmd_packet = MakeReadRemoteSupportedFeatures(0x0001);
  cmd_id = cmd_channel()->SendCommand(
      std::move(cmd_packet),
      std::move(cmd_cb),
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, cmd_id);

  RunUntilIdle();

  EXPECT_EQ(0, cmd_events);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));

  RunUntilIdle();

  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesRsp);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  // The command should have gotten update and complete events.
  EXPECT_EQ(2, cmd_events);
}

// Tests:
//  - Calling RemoveQueuedCommand on an asynchronous command that has already
//  been sent to the
//    controller and gotten Command Status returns false.
//  - The command still notifies the callback for completion event.
TEST_F(CommandChannelTest, RemoveQueuedAsyncCommandPendingCompletion) {
  EXPECT_CMD_PACKET_OUT(test_device(),
                        kReadRemoteSupportedFeaturesCmd,
                        &kReadRemoteSupportedFeaturesRsp);

  int transaction_count = 0;
  test_device()->SetTransactionCallback(
      [&transaction_count] { transaction_count++; });

  CommandChannel::TransactionId cmd_id;
  int cmd_events = 0;
  auto cmd_cb = [&cmd_id, &cmd_events](CommandChannel::TransactionId id,
                                       const EventPacket& event) {
    EXPECT_EQ(cmd_id, id);
    if (cmd_events == 0) {
      EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
    }
    cmd_events++;
  };

  auto cmd_packet = MakeReadRemoteSupportedFeatures(0x0001);
  cmd_id = cmd_channel()->SendCommand(
      std::move(cmd_packet),
      std::move(cmd_cb),
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
  EXPECT_NE(0u, cmd_id);

  RunUntilIdle();

  EXPECT_EQ(1, cmd_events);
  EXPECT_FALSE(cmd_channel()->RemoveQueuedCommand(cmd_id));

  RunUntilIdle();

  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RunUntilIdle();

  EXPECT_EQ(1, transaction_count);
  // The command should have gotten update and complete events.
  EXPECT_EQ(2, cmd_events);
}

TEST_F(CommandChannelTest, VendorEventHandler) {
  constexpr hci_spec::EventCode kTestSubeventCode0 = 0x10;
  constexpr hci_spec::EventCode kTestSubeventCode1 = 0x12;
  StaticByteBuffer vendor_event_bytes0(
      hci_spec::kVendorDebugEventCode, 0x01, kTestSubeventCode0);
  auto vendor_event_bytes1 = StaticByteBuffer(
      hci_spec::kVendorDebugEventCode, 0x01, kTestSubeventCode1);

  int event_count0 = 0;
  auto event_cb0 = [&event_count0,
                    kTestSubeventCode0](const EmbossEventPacket& event) {
    event_count0++;
    EXPECT_EQ(hci_spec::kVendorDebugEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode0,
              event.view<pw::bluetooth::emboss::VendorDebugEventView>()
                  .subevent_code()
                  .Read());
    return EventCallbackResult::kContinue;
  };

  int event_count1 = 0;
  auto event_cb1 = [&event_count1,
                    kTestSubeventCode1](const EmbossEventPacket& event) {
    event_count1++;
    EXPECT_EQ(hci_spec::kVendorDebugEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode1,
              event.view<pw::bluetooth::emboss::VendorDebugEventView>()
                  .subevent_code()
                  .Read());
    return EventCallbackResult::kContinue;
  };

  auto id0 =
      cmd_channel()->AddVendorEventHandler(kTestSubeventCode0, event_cb0);
  EXPECT_NE(0u, id0);

  // Can register a handler for the same event code more than once.
  auto id1 =
      cmd_channel()->AddVendorEventHandler(kTestSubeventCode0, event_cb0);
  EXPECT_NE(0u, id1);
  EXPECT_NE(id0, id1);

  // Add a handler for a different event code.
  auto id2 =
      cmd_channel()->AddVendorEventHandler(kTestSubeventCode1, event_cb1);
  EXPECT_NE(0u, id2);

  test_device()->SendCommandChannelPacket(vendor_event_bytes0);
  RunUntilIdle();
  EXPECT_EQ(2, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(vendor_event_bytes0);
  RunUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(vendor_event_bytes1);
  RunUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(1, event_count1);

  // Remove the first event handler.
  cmd_channel()->RemoveEventHandler(id0);
  test_device()->SendCommandChannelPacket(vendor_event_bytes0);
  test_device()->SendCommandChannelPacket(vendor_event_bytes1);
  RunUntilIdle();
  EXPECT_EQ(5, event_count0);
  EXPECT_EQ(2, event_count1);
}

TEST_F(CommandChannelTest, LEMetaEventHandler) {
  constexpr hci_spec::EventCode kTestSubeventCode0 = 0xFE;
  constexpr hci_spec::EventCode kTestSubeventCode1 = 0xFF;
  auto le_meta_event_bytes0 =
      StaticByteBuffer(hci_spec::kLEMetaEventCode, 0x01, kTestSubeventCode0);
  auto le_meta_event_bytes1 =
      StaticByteBuffer(hci_spec::kLEMetaEventCode, 0x01, kTestSubeventCode1);

  int event_count0 = 0;
  auto event_cb0 = [&event_count0,
                    kTestSubeventCode0](const EventPacket& event) {
    event_count0++;
    EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode0,
              event.params<hci_spec::LEMetaEventParams>().subevent_code);
    return EventCallbackResult::kContinue;
  };

  int event_count1 = 0;
  auto event_cb1 = [&event_count1,
                    kTestSubeventCode1](const EventPacket& event) {
    event_count1++;
    EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestSubeventCode1,
              event.params<hci_spec::LEMetaEventParams>().subevent_code);
    return EventCallbackResult::kContinue;
  };

  auto id0 =
      cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode0, event_cb0);
  EXPECT_NE(0u, id0);

  // Can register a handler for the same event code more than once.
  auto id1 =
      cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode0, event_cb0);
  EXPECT_NE(0u, id1);
  EXPECT_NE(id0, id1);

  // Add a handler for a different event code.
  auto id2 =
      cmd_channel()->AddLEMetaEventHandler(kTestSubeventCode1, event_cb1);
  EXPECT_NE(0u, id2);

  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  RunUntilIdle();
  EXPECT_EQ(2, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  RunUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(0, event_count1);

  test_device()->SendCommandChannelPacket(le_meta_event_bytes1);
  RunUntilIdle();
  EXPECT_EQ(4, event_count0);
  EXPECT_EQ(1, event_count1);

  // Remove the first event handler.
  cmd_channel()->RemoveEventHandler(id0);
  test_device()->SendCommandChannelPacket(le_meta_event_bytes0);
  test_device()->SendCommandChannelPacket(le_meta_event_bytes1);
  RunUntilIdle();
  EXPECT_EQ(5, event_count0);
  EXPECT_EQ(2, event_count1);
}

TEST_F(CommandChannelTest, EventHandlerIdsDontCollide) {
  // Add a LE Meta event handler and a event handler and make sure that IDs are
  // generated correctly across the two methods.
  EXPECT_EQ(1u,
            cmd_channel()->AddLEMetaEventHandler(
                hci_spec::kLEConnectionCompleteSubeventCode,
                [](const EmbossEventPacket&) {
                  return EventCallbackResult::kContinue;
                }));
  EXPECT_EQ(
      2u,
      cmd_channel()->AddEventHandler(
          hci_spec::kDisconnectionCompleteEventCode,
          [](const EventPacket&) { return EventCallbackResult::kContinue; }));
}

// Tests:
//  - Can't register an event handler for CommandStatus or CommandComplete
TEST_F(CommandChannelTest, EventHandlerRestrictions) {
  auto id0 = cmd_channel()->AddEventHandler(
      hci_spec::kCommandStatusEventCode,
      [](const EventPacket&) { return EventCallbackResult::kContinue; });
  EXPECT_EQ(0u, id0);
  id0 = cmd_channel()->AddEventHandler(
      hci_spec::kCommandCompleteEventCode,
      [](const EventPacket&) { return EventCallbackResult::kContinue; });
  EXPECT_EQ(0u, id0);
}

// Tests that an asynchronous command with a completion event code does not
// remove an existing handler for colliding LE meta subevent code.
TEST_F(CommandChannelTest,
       AsyncEventHandlersAndLeMetaEventHandlersDoNotInterfere) {
  // Set up expectations for the asynchronous command and its corresponding
  // command status event.
  // clang-format off
  auto cmd = StaticByteBuffer(
      LowerBits(hci_spec::kInquiry), UpperBits(hci_spec::kInquiry),  // HCI_Inquiry opcode
      0x00                                       // parameter_total_size
  );
  auto cmd_status = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0x01, // status, num_hci_command_packets (1 can be sent)
      LowerBits(hci_spec::kInquiry), UpperBits(hci_spec::kInquiry)  // HCI_Inquiry opcode
  );
  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), cmd, &cmd_status);

  constexpr hci_spec::EventCode kTestEventCode = 0x01;

  // Add LE event handler for kTestEventCode
  int le_event_count = 0;
  auto le_event_cb = [&](const EventPacket& event) {
    EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
    EXPECT_EQ(kTestEventCode,
              event.params<hci_spec::LEMetaEventParams>().subevent_code);
    le_event_count++;
    return EventCallbackResult::kContinue;
  };
  cmd_channel()->AddLEMetaEventHandler(
      hci_spec::kLEConnectionCompleteSubeventCode, std::move(le_event_cb));

  // Initiate the async transaction with kTestEventCode as its completion code
  // (we use hci_spec::kInquiry as a test opcode).
  int async_cmd_cb_count = 0;
  auto async_cmd_cb = [&](auto id, const EventPacket& event) {
    if (async_cmd_cb_count == 0) {
      EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
    } else {
      EXPECT_EQ(kTestEventCode, event.event_code());
    }
    async_cmd_cb_count++;
  };

  auto packet =
      EmbossCommandPacket::New<pw::bluetooth::emboss::InquiryCommandView>(
          hci_spec::kInquiry,
          pw::bluetooth::emboss::CommandHeader::IntrinsicSizeInBytes());
  cmd_channel()->SendCommand(
      std::move(packet), std::move(async_cmd_cb), kTestEventCode);

  // clang-format off
  auto event_bytes = StaticByteBuffer(
      kTestEventCode,
      0x01,  // parameter_total_size
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto le_event_bytes = StaticByteBuffer(
      hci_spec::kLEMetaEventCode,
      0x01,  // parameter_total_size
      kTestEventCode);
  // clang-format on

  // Send a spurious LE event before processing the Command Status event. This
  // should get routed to the correct event handler.
  test_device()->SendCommandChannelPacket(le_event_bytes);

  // Process the async command expectation.
  RunUntilIdle();

  // End the asynchronous transaction. This should NOT unregister the LE event
  // handler.
  test_device()->SendCommandChannelPacket(event_bytes);

  // Send more LE events. These should get routed to the LE event handler.
  test_device()->SendCommandChannelPacket(le_event_bytes);
  test_device()->SendCommandChannelPacket(le_event_bytes);

  RunUntilIdle();

  // Should have received 3 LE events.
  EXPECT_EQ(3, le_event_count);

  // The async command handler should have been called twice: once for Command
  // Status and once for the completion event.
  EXPECT_EQ(2, async_cmd_cb_count);
}

TEST_F(CommandChannelTest, TransportClosedCallback) {
  bool error_cb_called = false;
  auto error_cb = [&error_cb_called] { error_cb_called = true; };
  transport()->SetTransportErrorCallback(error_cb);

  (void)heap_dispatcher().Post(
      [this](pw::async::Context /*ctx*/, pw::Status status) {
        if (status.ok()) {
          test_device()->Stop();
        }
      });
  RunUntilIdle();
  EXPECT_TRUE(error_cb_called);
}

TEST_F(CommandChannelTest, CommandTimeoutCallback) {
  auto req_reset =
      StaticByteBuffer(LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                       0x00                          // parameter_total_size
      );

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, );

  size_t timeout_cb_count = 0;
  auto timeout_cb = [&] { timeout_cb_count++; };
  cmd_channel()->set_channel_timeout_cb(timeout_cb);

  size_t cmd_cb_count = 0;
  auto cb = [&](auto, auto&) { cmd_cb_count++; };

  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id1 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id1);

  packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id2 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id2);

  // Run the loop until the command timeout task gets scheduled.
  RunUntilIdle();
  EXPECT_EQ(0u, timeout_cb_count);
  EXPECT_EQ(0u, cmd_cb_count);

  RunFor(kCommandTimeout);

  EXPECT_EQ(1u, timeout_cb_count);
  EXPECT_EQ(0u, cmd_cb_count);

  DeleteTransport();
  EXPECT_EQ(0u, cmd_cb_count);
}

TEST_F(CommandChannelTest, DestroyChannelInTimeoutCallback) {
  auto req_reset =
      StaticByteBuffer(LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                       0x00                          // parameter_total_size
      );

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, );

  size_t timeout_cb_count = 0;
  auto timeout_cb = [&] {
    timeout_cb_count++;
    DeleteTransport();
  };
  cmd_channel()->set_channel_timeout_cb(timeout_cb);

  size_t cmd_cb_count = 0;
  auto cb = [&](auto, auto&) { cmd_cb_count++; };

  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id1 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id1);

  packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id2 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id2);

  RunFor(kCommandTimeout);
  EXPECT_EQ(1u, timeout_cb_count);
}

TEST_F(CommandChannelTest, CommandsAndEventsIgnoredAfterCommandTimeout) {
  size_t timeout_cb_count = 0;
  auto timeout_cb = [&] { timeout_cb_count++; };
  cmd_channel()->set_channel_timeout_cb(timeout_cb);

  size_t cmd_cb_count = 0;
  auto cb = [&](auto, auto&) { cmd_cb_count++; };

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  auto req_reset =
      StaticByteBuffer(LowerBits(hci_spec::kReset),
                       UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                       0x00                          // parameter_total_size
      );
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset);
  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id1 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id1);

  // Run the loop until the command timeout task gets scheduled.
  RunUntilIdle();
  EXPECT_EQ(0u, timeout_cb_count);
  RunFor(kCommandTimeout);
  EXPECT_EQ(1u, timeout_cb_count);
  EXPECT_EQ(0u, cmd_cb_count);

  // Additional commands should be ignored.
  packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  CommandChannel::TransactionId id2 =
      cmd_channel()->SendCommand(std::move(packet), cb);
  EXPECT_EQ(0u, id2);
  // No command should be sent.
  RunUntilIdle();

  // Events should be ignored.
  test_device()->SendCommandChannelPacket(bt::testing::CommandCompletePacket(
      hci_spec::kReset, pw::bluetooth::emboss::StatusCode::SUCCESS));
  RunUntilIdle();
  EXPECT_EQ(0u, cmd_cb_count);
}

// Tests:
//  - Asynchronous commands should be able to schedule another asynchronous
//    command in their callback.
TEST_F(CommandChannelTest, AsynchronousCommandChaining) {
  constexpr size_t kExpectedCallbacksPerCommand = 2;
  constexpr hci_spec::EventCode kTestEventCode0 = 0xFE;
  // Set up expectations
  // clang-format off
  // Using HCI_Reset for testing.
  auto req_reset = StaticByteBuffer(
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset), // HCI_Reset opcode
      0x00                                  // parameter_total_size (no payload)
      );
  auto rsp_resetstatus = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,                        // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA,  // status, num_hci_command_packets (250)
      LowerBits(hci_spec::kReset), UpperBits(hci_spec::kReset)  // HCI_Reset opcode
  );
  auto req_inqcancel = StaticByteBuffer(
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel), // HCI_InquiryCancel
      0x00                        // parameter_total_size (no payload)
  );
  auto rsp_inqstatus = StaticByteBuffer(
       hci_spec::kCommandStatusEventCode,
      0x04,                        // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::SUCCESS, 0xFA,  // status, num_hci_command_packets (250)
      LowerBits(hci_spec::kInquiryCancel), UpperBits(hci_spec::kInquiryCancel) // HCI_InquiryCanacel
  );
  auto rsp_bogocomplete = StaticByteBuffer(
      kTestEventCode0,
      0x00 // parameter_total_size (no payload)
      );
  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, &rsp_resetstatus);
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset, &rsp_resetstatus);

  CommandChannel::TransactionId id1, id2;
  CommandChannel::CommandCallback cb;
  size_t cb_count = 0u;

  cb = [&cb,
        cmd_channel = cmd_channel(),
        &id1,
        &id2,
        &cb_count,
        kTestEventCode0](CommandChannel::TransactionId callback_id,
                         const EventPacket& event) {
    if (cb_count < kExpectedCallbacksPerCommand) {
      EXPECT_EQ(id1, callback_id);
    } else {
      EXPECT_EQ(id2, callback_id);
    }
    if ((cb_count % 2) == 0) {
      // First event from each command - CommandStatus
      EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
      auto params = event.params<hci_spec::CommandStatusEventParams>();
      EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
    } else {
      // Second event from each command - completion event
      EXPECT_EQ(kTestEventCode0, event.event_code());
      if (cb_count < 2) {
        // Add the second command when the first one completes.
        auto packet = hci::EmbossCommandPacket::New<
            pw::bluetooth::emboss::ResetCommandWriter>(hci_spec::kReset);
        id2 = cmd_channel->SendCommand(
            std::move(packet), cb.share(), kTestEventCode0);
      }
    }
    cb_count++;
  };

  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  id1 = cmd_channel()->SendCommand(
      std::move(packet), cb.share(), kTestEventCode0);

  RunUntilIdle();

  // Should have received the Status but not the result.
  EXPECT_EQ(1u, cb_count);

  // Sending the complete will finish the command and add the next command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunUntilIdle();

  EXPECT_EQ(3u, cb_count);

  // Finish out the command.
  test_device()->SendCommandChannelPacket(rsp_bogocomplete);
  RunUntilIdle();

  EXPECT_EQ(4u, cb_count);
}

// Tests:
//  - Commands that are exclusive of other commands cannot run together, and
//    instead wait until the exclusive commands finish.
//  - Exclusive Commands in the queue still get started in order
//  - Commands that aren't exclusive run as normal even when an exclusive one is
//    waiting.
TEST_F(CommandChannelTest, ExclusiveCommands) {
  constexpr hci_spec::EventCode kExclOneCompleteEvent = 0xFE;
  constexpr hci_spec::EventCode kExclTwoCompleteEvent = 0xFD;
  constexpr hci_spec::OpCode kExclusiveOne = hci_spec::DefineOpCode(0x01, 0x01);
  constexpr hci_spec::OpCode kExclusiveTwo = hci_spec::DefineOpCode(0x01, 0x02);
  constexpr hci_spec::OpCode kNonExclusive = hci_spec::DefineOpCode(0x01, 0x03);

  // Set up expectations
  //  - kExclusiveOne can't run at the same time as kExclusiveTwo, and
  //  vice-versa.
  //  - kExclusiveOne finishes with kExclOneCompleteEvent
  //  - kExclusiveTwo finishes with kExclTwoCompleteEvent
  //  - kNonExclusive can run whenever it wants.
  //  - For testing, we omit the payloads of all commands.
  auto excl_one_cmd = StaticByteBuffer(
      LowerBits(kExclusiveOne), UpperBits(kExclusiveOne), 0x00  // (no payload)
  );
  auto rsp_excl_one_status =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,  // status, num_hci_command_packets (250)
                       LowerBits(kExclusiveOne),
                       UpperBits(kExclusiveOne)  // HCI opcode
      );
  auto rsp_one_complete = StaticByteBuffer(
      kExclOneCompleteEvent, 0x00  // parameter_total_size (no payload)
  );

  auto excl_two_cmd = StaticByteBuffer(
      LowerBits(kExclusiveTwo), UpperBits(kExclusiveTwo), 0x00  // (no payload)
  );
  auto rsp_excl_two_status =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,  // status, num_hci_command_packets (250)
                       LowerBits(kExclusiveTwo),
                       UpperBits(kExclusiveTwo)  // HCI opcode
      );
  auto rsp_two_complete = StaticByteBuffer(
      kExclTwoCompleteEvent, 0x00  // parameter_total_size (no payload)
  );

  auto nonexclusive_cmd =
      StaticByteBuffer(LowerBits(kNonExclusive),
                       UpperBits(kNonExclusive),  // HCI opcode
                       0x00  // parameter_total_size (no payload)
      );
  auto nonexclusive_complete = StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      0xFA,  // num_hci_command_packets (250)
      LowerBits(kNonExclusive),
      UpperBits(kNonExclusive),                   // HCI opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS  // Command succeeded
  );

  CommandChannel::TransactionId id1, id2, id3;
  CommandChannel::CommandCallback exclusive_cb;
  size_t exclusive_cb_count = 0u;

  size_t nonexclusive_cb_count = 0;
  CommandChannel::CommandCallback nonexclusive_cb =
      [&nonexclusive_cb_count](auto callback_id, const EventPacket& event) {
        EXPECT_EQ(hci_spec::kCommandCompleteEventCode, event.event_code());
        nonexclusive_cb_count++;
      };

  exclusive_cb = [&exclusive_cb,
                  &nonexclusive_cb,
                  cmd_channel = cmd_channel(),
                  &id1,
                  &id2,
                  &id3,
                  &exclusive_cb_count,
                  kExclOneCompleteEvent,
                  kExclTwoCompleteEvent](
                     CommandChannel::TransactionId callback_id,
                     const EventPacket& event) {
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
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        auto params = event.params<hci_spec::CommandStatusEventParams>();
        EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
        auto packet = CommandPacket::New(kExclusiveTwo);
        id2 = cmd_channel->SendExclusiveCommand(std::move(packet),
                                                exclusive_cb.share(),
                                                kExclTwoCompleteEvent,
                                                {kExclusiveOne});
        std::cout << "queued Exclusive Two: " << id2 << std::endl;
        break;
      }
      case 1: {
        // Complete for kExclusiveOne -> Resend kExclusiveOne
        EXPECT_EQ(id1, callback_id);
        EXPECT_EQ(kExclOneCompleteEvent, event.event_code());
        // Add the second command when the first one completes.
        auto packet = CommandPacket::New(kExclusiveOne);
        id3 = cmd_channel->SendExclusiveCommand(std::move(packet),
                                                exclusive_cb.share(),
                                                kExclOneCompleteEvent,
                                                {kExclusiveTwo});
        std::cout << "queued Second Exclusive One: " << id3 << std::endl;
        packet = CommandPacket::New(kNonExclusive);
        cmd_channel->SendCommand(std::move(packet), nonexclusive_cb.share());

        break;
      }
      case 2: {  // Status for kExclusiveTwo
        EXPECT_EQ(id2, callback_id);
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        auto params = event.params<hci_spec::CommandStatusEventParams>();
        EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
        break;
      }
      case 3: {  // Complete for kExclusiveTwo
        EXPECT_EQ(id2, callback_id);
        EXPECT_EQ(kExclTwoCompleteEvent, event.event_code());
        break;
      }
      case 4: {  // Status for Second kExclusiveOne
        EXPECT_EQ(id3, callback_id);
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        auto params = event.params<hci_spec::CommandStatusEventParams>();
        EXPECT_EQ(pw::bluetooth::emboss::StatusCode::SUCCESS, params.status);
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

  EXPECT_CMD_PACKET_OUT(test_device(), excl_one_cmd, &rsp_excl_one_status);
  EXPECT_CMD_PACKET_OUT(
      test_device(), nonexclusive_cmd, &nonexclusive_complete);
  id1 = cmd_channel()->SendExclusiveCommand(CommandPacket::New(kExclusiveOne),
                                            exclusive_cb.share(),
                                            kExclOneCompleteEvent,
                                            {kExclusiveTwo});
  cmd_channel()->SendCommand(CommandPacket::New(kNonExclusive),
                             nonexclusive_cb.share());
  RunUntilIdle();
  // Should have received the ExclusiveOne status but not the complete.
  // ExclusiveTwo should be queued.
  EXPECT_EQ(1u, exclusive_cb_count);
  // NonExclusive should be completed.
  EXPECT_EQ(1u, nonexclusive_cb_count);

  // Sending the ExclusiveOne complete will send the ExclusiveTwo command, queue
  // another ExclusiveOne command, and send a NonExclusive command.
  EXPECT_CMD_PACKET_OUT(test_device(), excl_two_cmd, &rsp_excl_two_status);
  EXPECT_CMD_PACKET_OUT(
      test_device(), nonexclusive_cmd, &nonexclusive_complete);
  test_device()->SendCommandChannelPacket(rsp_one_complete);
  RunUntilIdle();
  EXPECT_EQ(3u,
            exclusive_cb_count);  // +2: rsp_one_complete, rsp_excl_two_status
  EXPECT_EQ(2u, nonexclusive_cb_count);  // +1: nonexclusive_complete

  // Complete ExclusiveTwo and send a NonExclusive. The queued ExclusiveOne
  // should be sent.
  EXPECT_CMD_PACKET_OUT(
      test_device(), nonexclusive_cmd, &nonexclusive_complete);
  EXPECT_CMD_PACKET_OUT(test_device(), excl_one_cmd, &rsp_excl_one_status);
  test_device()->SendCommandChannelPacket(rsp_two_complete);
  cmd_channel()->SendCommand(CommandPacket::New(kNonExclusive),
                             nonexclusive_cb.share());
  RunUntilIdle();
  EXPECT_EQ(5u,
            exclusive_cb_count);  // +2: rsp_two_complete, rsp_excl_one_status
  EXPECT_EQ(3u, nonexclusive_cb_count);  // +1: nonexclusive_complete

  // Finish the second ExclusiveOne
  test_device()->SendCommandChannelPacket(rsp_one_complete);
  RunUntilIdle();
  EXPECT_EQ(6u, exclusive_cb_count);  // +1: rsp_one_complete
  EXPECT_EQ(3u, nonexclusive_cb_count);
}

TEST_F(CommandChannelTest, SendCommandFailsIfEventHandlerInstalled) {
  constexpr hci_spec::EventCode kTestEventCode0 = 0xFE;

  // Register event handler for kTestEventCode0.
  auto id0 = cmd_channel()->AddEventHandler(
      kTestEventCode0,
      [](const EventPacket& event) { return EventCallbackResult::kContinue; });
  EXPECT_NE(0u, id0);

  // Try to send a command for kTestEventCode0. SendCommand should fail for a
  // code already registered with "AddEventHandler".
  auto reset =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::ResetCommandWriter>(
          hci_spec::kReset);
  auto transaction_id = cmd_channel()->SendCommand(
      std::move(reset), [](auto, const auto&) {}, kTestEventCode0);
  EXPECT_EQ(0u, transaction_id);
}

TEST_F(CommandChannelTest, EventHandlerResults) {
  constexpr hci_spec::EventCode kTestEventCode0 = 0xFE;

  int event_count = 0;
  auto event_cb = [&event_count, kTestEventCode0](const EventPacket& event) {
    event_count++;
    EXPECT_EQ(kTestEventCode0, event.event_code());

    if (event_count == 1) {
      return EventCallbackResult::kContinue;
    }

    return EventCallbackResult::kRemove;
  };

  EXPECT_NE(cmd_channel()->AddEventHandler(kTestEventCode0, event_cb), 0u);

  // Send three requests, and process the callbacks immediately. The second
  // callback returns "remove" before the third event callback has been called.
  auto event0 = StaticByteBuffer(kTestEventCode0, 0x00);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  test_device()->SendCommandChannelPacket(event0);
  RunUntilIdle();
  EXPECT_EQ(2, event_count);
}

TEST_F(CommandChannelTest, SendCommandWithLEMetaEventSubeventRsp) {
  constexpr hci_spec::OpCode kOpCode = hci_spec::kLEReadRemoteFeatures;
  constexpr hci_spec::EventCode kSubeventCode =
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd = StaticByteBuffer(LowerBits(kOpCode),
                              UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);

  auto cmd_status_event =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       // parameter total size (4 byte payload)
                       0x04,
                       // status, num_hci_command_packets (250)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,
                       // HCI opcode
                       LowerBits(kOpCode),
                       UpperBits(kOpCode));
  auto cmd_complete_subevent =
      StaticByteBuffer(hci_spec::kLEMetaEventCode,
                       0x01,  // parameter total size (1 byte payload)
                       kSubeventCode);

  EXPECT_CMD_PACKET_OUT(test_device(), cmd, &cmd_status_event);

  auto cmd_packet = CommandPacket::New(kOpCode);

  size_t event_count = 0;
  auto event_cb = [&event_count](auto, const EventPacket& event) {
    switch (event_count) {
      case 0: {
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count++;
  };
  auto id = cmd_channel()->SendLeAsyncCommand(
      std::move(cmd_packet), std::move(event_cb), kSubeventCode);
  EXPECT_NE(0u, id);

  RunUntilIdle();
  EXPECT_EQ(1u, event_count);

  // Handler should be removed when subevent received.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunUntilIdle();
  EXPECT_EQ(2u, event_count);

  // This seconod complete event should be ignored because the handler should
  // have been removed.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunUntilIdle();
  EXPECT_EQ(2u, event_count);
}

TEST_F(
    CommandChannelTest,
    SendingLECommandAfterAddingLEMetaEventHandlerFailsForSameSubeventCodeAndSucceedsForDifferentSubeventCode) {
  constexpr hci_spec::EventCode kSubeventCode =
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode;
  constexpr hci_spec::OpCode kOpCode =
      hci_spec::kLEReadRemoteFeatures;  // LE Read Remote Features

  EXPECT_NE(0u,
            cmd_channel()->AddLEMetaEventHandler(
                kSubeventCode, [](const EmbossEventPacket&) {
                  return EventCallbackResult::kContinue;
                }));
  EXPECT_EQ(0u,
            cmd_channel()->SendLeAsyncCommand(
                CommandPacket::New(kOpCode),
                [](auto, const auto&) {},
                kSubeventCode));

  auto cmd = StaticByteBuffer(LowerBits(kOpCode),
                              UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);
  EXPECT_CMD_PACKET_OUT(test_device(), std::move(cmd), );
  EXPECT_NE(0u,
            cmd_channel()->SendLeAsyncCommand(
                CommandPacket::New(kOpCode),
                [](auto, const auto&) {},
                kSubeventCode + 1));
  RunUntilIdle();
}

TEST_F(CommandChannelTest,
       SendingSecondLECommandWithSameSubeventShouldWaitForFirstToComplete) {
  // Commands have different op codes but same subevent code so that second
  // command is not blocked because of matching op codes (which would not test
  // LE command handling).
  constexpr hci_spec::OpCode kOpCode0 = hci_spec::kLEReadRemoteFeatures;
  constexpr hci_spec::OpCode kOpCode1 = hci_spec::kLEReadBufferSizeV1;
  constexpr hci_spec::EventCode kSubeventCode =
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd0 = StaticByteBuffer(LowerBits(kOpCode0),
                               UpperBits(kOpCode0),
                               // parameter total size (0 byte payload)
                               0x00);
  auto cmd0_status_event =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       // parameter total size (4 byte payload)
                       0x04,
                       // status, num_hci_command_packets (250)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,
                       // HCI opcode
                       LowerBits(kOpCode0),
                       UpperBits(kOpCode0));
  auto cmd1 = StaticByteBuffer(LowerBits(kOpCode1),
                               UpperBits(kOpCode1),
                               // parameter total size (0 byte payload)
                               0x00);
  auto cmd1_status_event =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       // parameter total size (4 byte payload)
                       0x04,
                       // status, num_hci_command_packets (250)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,
                       // HCI opcode
                       LowerBits(kOpCode1),
                       UpperBits(kOpCode1));

  auto cmd_complete_subevent =
      StaticByteBuffer(hci_spec::kLEMetaEventCode,
                       0x01,  // parameter total size (1 byte payload)
                       kSubeventCode);

  EXPECT_CMD_PACKET_OUT(test_device(), cmd0, &cmd0_status_event);

  size_t event_count_0 = 0;
  auto event_cb_0 = [&event_count_0](auto, const EventPacket& event) {
    switch (event_count_0) {
      case 0: {
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count_0++;
  };
  auto id_0 = cmd_channel()->SendLeAsyncCommand(
      CommandPacket::New(kOpCode0), std::move(event_cb_0), kSubeventCode);
  EXPECT_NE(0u, id_0);

  RunUntilIdle();
  EXPECT_EQ(1u, event_count_0);

  size_t event_count_1 = 0;
  auto event_cb_1 = [&event_count_1](auto, const EventPacket& event) {
    switch (event_count_1) {
      case 0: {
        EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
        break;
      }
      case 1: {
        EXPECT_EQ(hci_spec::kLEMetaEventCode, event.event_code());
        break;
      }
      default: {
        FAIL();
      }
    }
    event_count_1++;
  };
  // Command should be queued and not sent until after first complete event
  // received.
  auto id_1 = cmd_channel()->SendLeAsyncCommand(
      CommandPacket::New(kOpCode1), std::move(event_cb_1), kSubeventCode);
  EXPECT_NE(0u, id_1);
  RunUntilIdle();
  EXPECT_EQ(0u, event_count_1);

  // When first command complete event is received, second command should be
  // sent.
  EXPECT_CMD_PACKET_OUT(test_device(), cmd1, &cmd1_status_event);
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunUntilIdle();
  EXPECT_EQ(2u, event_count_0);
  EXPECT_EQ(1u, event_count_1);

  // Second complete event should be received by second command event handler
  // only.
  test_device()->SendCommandChannelPacket(cmd_complete_subevent);
  RunUntilIdle();
  EXPECT_EQ(2u, event_count_0);
  EXPECT_EQ(2u, event_count_1);
}

TEST_F(
    CommandChannelTest,
    RegisteringLEMetaEventHandlerWhileLECommandPendingFailsForSameSubeventAndSucceedsForDifferentSubevent) {
  constexpr hci_spec::OpCode kOpCode = hci_spec::kLEReadRemoteFeatures;
  constexpr hci_spec::EventCode kSubeventCode =
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode;

  auto cmd = StaticByteBuffer(LowerBits(kOpCode),
                              UpperBits(kOpCode),
                              // parameter total size (0 byte payload)
                              0x00);

  auto cmd_status_event =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       // parameter total size (4 byte payload)
                       0x04,
                       // status, num_hci_command_packets (250)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       0xFA,
                       // HCI opcode
                       LowerBits(kOpCode),
                       UpperBits(kOpCode));

  EXPECT_CMD_PACKET_OUT(test_device(), cmd, &cmd_status_event);

  size_t event_count = 0;
  auto event_cb = [&event_count](auto, const EventPacket& event) {
    EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());
    event_count++;
  };
  auto id = cmd_channel()->SendLeAsyncCommand(
      CommandPacket::New(kOpCode), std::move(event_cb), kSubeventCode);
  EXPECT_NE(0u, id);
  RunUntilIdle();
  EXPECT_EQ(1u, event_count);

  // Async LE command for subevent is already pending, so registering event
  // handler should fail by returning 0.
  id = cmd_channel()->AddLEMetaEventHandler(
      kSubeventCode,
      [](const EmbossEventPacket&) { return EventCallbackResult::kContinue; });
  EXPECT_EQ(0u, id);

  // Registering event handler for different subevent code should succeed.
  id = cmd_channel()->AddLEMetaEventHandler(
      kSubeventCode + 1,
      [](const EmbossEventPacket&) { return EventCallbackResult::kContinue; });
  EXPECT_NE(0u, id);
}

#ifndef NINSPECT
TEST_F(CommandChannelTest, InspectHierarchy) {
  cmd_channel()->AttachInspect(inspector_.GetRoot(), "command_channel");

  auto command_channel_matcher = AllOf(NodeMatches(AllOf(
      NameMatches("command_channel"),
      PropertyList(UnorderedElementsAre(UintIs("allowed_command_packets", 1),
                                        UintIs("next_event_handler_id", 1),
                                        UintIs("next_transaction_id", 1))))));

  EXPECT_THAT(inspect::ReadFromVmo(inspector_.DuplicateVmo()).value(),
              ChildrenMatch(ElementsAre(command_channel_matcher)));
}
#endif  // NINSPECT

}  // namespace
}  // namespace bt::hci
