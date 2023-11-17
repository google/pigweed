// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::hci {
namespace {

constexpr hci_spec::OpCode kTestOpCode = 0xFFFF;
constexpr hci_spec::OpCode kTestOpCode2 = 0xF00F;

using bt::testing::CommandTransaction;

using TestingBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

class SequentialCommandRunnerTest : public TestingBase {
 public:
  SequentialCommandRunnerTest() = default;
  ~SequentialCommandRunnerTest() override = default;
};

using HCI_SequentialCommandRunnerTest = SequentialCommandRunnerTest;

TEST_F(SequentialCommandRunnerTest, SequentialCommandRunner) {
  // HCI command with custom opcode FFFF.
  StaticByteBuffer command_bytes(0xFF, 0xFF, 0x00);

  StaticByteBuffer command_status_error_bytes(
      hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE,
      1,
      0xFF,
      0xFF);

  StaticByteBuffer command_cmpl_error_bytes(
      hci_spec::kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      1,
      0xFF,
      0xFF,
      pw::bluetooth::emboss::StatusCode::RESERVED_0);

  auto command_cmpl_success_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       1,
                       0xFF,
                       0xFF,
                       pw::bluetooth::emboss::StatusCode::SUCCESS);

  // Here we perform multiple test sequences where we queue up several  commands
  // in each sequence. We expect each sequence to terminate differently after
  // the following HCI transactions:
  //
  // Sequence 1 (HCI packets)
  //    -> Command; <- error status
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_status_error_bytes);

  // Sequence 2 (HCI packets)
  //    -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_error_bytes);

  // Sequence 3 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_error_bytes);

  // Sequence 4 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 5 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  // Sequence 1 (test)
  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE),
            status);
  cb_called = 0;

  // Sequence 2 (test)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(2, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::RESERVED_0), status);
  cb_called = 0;

  // Sequence 3 (test)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(3, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::RESERVED_0), status);
  cb_called = 0;

  // Sequence 4 (test)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(4, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
  cb_called = 0;
  status_cb_called = 0;

  // Sequence 5 (test) (no callback passed to QueueCommand)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode));
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode));

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
}

TEST_F(SequentialCommandRunnerTest, SequentialCommandRunnerCancel) {
  StaticByteBuffer command_bytes(0xFF, 0xFF, 0x00);

  auto command_cmpl_error_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       1,
                       0xFF,
                       0xFF,
                       pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE);

  auto command_cmpl_success_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       1,
                       0xFF,
                       0xFF,
                       pw::bluetooth::emboss::StatusCode::SUCCESS);

  // Sequence 1
  //   -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 2
  //   -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 3
  //   -> Command; <- success complete
  //   -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_error_bytes);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  // Sequence 1: Sequence will be cancelled after the first command.
  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  // Call RunCommands() and cancel the sequence immediately. The
  // first command will go out but no successive packets should be sent.
  // The status callback should be invoked
  // No command callbacks should be called.
  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  cmd_runner.Cancel();

  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(HostError::kCanceled), status);
  cb_called = 0;
  status_cb_called = 0;
  status = fit::ok();

  // Sequence 2: Sequence will be cancelled after first command. This tests
  // canceling a sequence from a CommandCompleteCallback.
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          [&](const EventPacket& event) {
                            bt_log(TRACE, "hci-test", "callback called");
                            cmd_runner.Cancel();
                            EXPECT_TRUE(cmd_runner.IsReady());
                            EXPECT_FALSE(cmd_runner.HasQueuedCommands());
                          });
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  // |status_cb| is expected to get called with kCanceled
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(HostError::kCanceled), status);
  cb_called = 0;
  status_cb_called = 0;
  status = fit::ok();

  // Sequence 3: Sequence will be cancelled after first command and immediately
  // followed by a second command which will fail. This tests canceling a
  // sequence and initiating a new one from a CommandCompleteCallback.
  cmd_runner.QueueCommand(
      CommandPacket::New(kTestOpCode), [&](const EventPacket& event) {
        cmd_runner.Cancel();
        EXPECT_TRUE(cmd_runner.IsReady());
        EXPECT_FALSE(cmd_runner.HasQueuedCommands());

        EXPECT_EQ(1, status_cb_called);
        EXPECT_EQ(ToResult(HostError::kCanceled), status);

        // Queue multiple commands (only one will execute since MockController
        // will send back an error status).
        cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
        cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                                cb);  // <-- Should not run
        cmd_runner.RunCommands(status_cb);
      });
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunUntilIdle();

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  // The cb queued from inside the first callback should have been called.
  EXPECT_EQ(1, cb_called);
  // The result callback should have been called with the failure result.
  EXPECT_EQ(2, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE),
            status);
}

TEST_F(SequentialCommandRunnerTest, ParallelCommands) {
  // Need to signal to the queue that we can run more than one command at once.
  auto command_status_queue_increase =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       250,
                       0x00,
                       0x00);
  // HCI command with custom opcode FFFF.
  StaticByteBuffer command_bytes(0xFF, 0xFF, 0x00);
  auto command_status_error_bytes =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE,
                       2,
                       0xFF,
                       0xFF);

  auto command_cmpl_error_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       2,
                       0xFF,
                       0xFF,
                       pw::bluetooth::emboss::StatusCode::RESERVED_0);

  auto command_cmpl_success_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       2,
                       0xFF,
                       0xFF,
                       pw::bluetooth::emboss::StatusCode::SUCCESS);

  // HCI command with custom opcode F00F.
  StaticByteBuffer command2_bytes(0x0F, 0xF0, 0x00);
  auto command2_status_error_bytes =
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE,
                       2,
                       0x0F,
                       0xF0);

  auto command2_cmpl_error_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       2,
                       0x0F,
                       0xF0,
                       pw::bluetooth::emboss::StatusCode::RESERVED_0);

  auto command2_cmpl_success_bytes =
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x04,  // parameter_total_size (4 byte payload)
                       2,
                       0x0F,
                       0xF0,
                       pw::bluetooth::emboss::StatusCode::SUCCESS);

  test_device()->SendCommandChannelPacket(command_status_queue_increase);

  // Parallel commands should all run before commands that require success.
  // command and command2 are answered in opposite order because they should be
  // sent simultaneously.
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, );
  EXPECT_CMD_PACKET_OUT(test_device(), command2_bytes, );
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);

  int cb_called = 0;
  auto cb = [&](const hci::EventPacket&) { cb_called++; };

  int status_cb_called = 0;
  Result<> status = ToResult(HostError::kFailed);
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode2), cb, /*wait=*/false);
  cmd_runner.QueueCommand(
      CommandPacket::New(kTestOpCode),
      [&](const hci::EventPacket&) {
        EXPECT_EQ(2, cb_called);
        cb_called++;
      },
      /*wait=*/true);
  // We can also queue to the end of the queue without the last one being a
  // wait.
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb, /*wait=*/false);
  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunUntilIdle();
  // The first two commands should have been sent but no responses are back yet.
  EXPECT_EQ(0, cb_called);

  // It should not matter if they get answered in opposite order.
  test_device()->SendCommandChannelPacket(command2_cmpl_success_bytes);
  test_device()->SendCommandChannelPacket(command_cmpl_success_bytes);
  RunUntilIdle();

  EXPECT_EQ(5, cb_called);
  EXPECT_EQ(fit::ok(), status);
  EXPECT_EQ(1, status_cb_called);
  cb_called = 0;
  status_cb_called = 0;

  // If any simultaneous commands fail, the sequence fails and the command
  // sequence is terminated.
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, );
  EXPECT_CMD_PACKET_OUT(test_device(), command2_bytes, );

  int cb_0_called = 0;
  auto cb_0 = [&](const hci::EventPacket&) { cb_0_called++; };
  int cb_1_called = 0;
  auto cb_1 = [&](const hci::EventPacket&) { cb_1_called++; };
  int cb_2_called = 0;
  auto cb_2 = [&](const hci::EventPacket&) { cb_2_called++; };
  cmd_runner.QueueCommand(
      CommandPacket::New(kTestOpCode), cb_0, /*wait=*/false);
  cmd_runner.QueueCommand(
      CommandPacket::New(kTestOpCode2), cb_1, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb_2);  // shouldn't run

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunUntilIdle();
  // The first two commands should have been sent but no responses are back yet.
  EXPECT_EQ(0, cb_0_called);
  EXPECT_EQ(0, cb_1_called);
  EXPECT_EQ(0, cb_2_called);

  test_device()->SendCommandChannelPacket(command_status_error_bytes);
  test_device()->SendCommandChannelPacket(command2_cmpl_success_bytes);
  RunUntilIdle();

  // Only the first command's callback should be called, as further callbacks
  // will be canceled due to the error status.
  EXPECT_EQ(1, cb_0_called);
  EXPECT_EQ(0, cb_1_called);
  EXPECT_EQ(0, cb_2_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE),
            status);
}

TEST_F(SequentialCommandRunnerTest, CommandCompletesOnStatusEvent) {
  auto command = bt::testing::EmptyCommandPacket(kTestOpCode);
  auto command0_status_event = bt::testing::CommandStatusPacket(
      kTestOpCode, pw::bluetooth::emboss::StatusCode::SUCCESS);

  auto command1 = bt::testing::EmptyCommandPacket(kTestOpCode2);
  auto command1_cmpl_event = bt::testing::CommandCompletePacket(
      kTestOpCode2, pw::bluetooth::emboss::StatusCode::SUCCESS);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_CMD_PACKET_OUT(test_device(), command, &command0_status_event);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb,
                          /*wait=*/false,
                          hci_spec::kCommandStatusEventCode);

  EXPECT_CMD_PACKET_OUT(test_device(), command1, &command1_cmpl_event);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode2), cb, /*wait=*/true);

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
}

TEST_F(SequentialCommandRunnerTest, AsyncCommands) {
  auto command = bt::testing::EmptyCommandPacket(hci_spec::kRemoteNameRequest);
  auto command0_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kRemoteNameRequest, pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command0_cmpl_event =
      bt::testing::RemoteNameRequestCompletePacket(DeviceAddress());

  auto command1 =
      bt::testing::EmptyCommandPacket(hci_spec::kLEReadRemoteFeatures);
  auto command1_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kLEReadRemoteFeatures,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command1_cmpl_event = bt::testing::LEReadRemoteFeaturesCompletePacket(
      /*conn=*/0x0000, hci_spec::LESupportedFeatures{0});

  auto command2 =
      bt::testing::EmptyCommandPacket(hci_spec::kReadRemoteVersionInfo);
  auto command2_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kReadRemoteVersionInfo,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command2_cmpl_event =
      bt::testing::ReadRemoteVersionInfoCompletePacket(/*conn=*/0x0000);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_CMD_PACKET_OUT(test_device(), command, &command0_status_event);
  cmd_runner.QueueCommand(CommandPacket::New(hci_spec::kRemoteNameRequest),
                          cb,
                          /*wait=*/false,
                          hci_spec::kRemoteNameRequestCompleteEventCode);

  EXPECT_CMD_PACKET_OUT(test_device(), command1, &command1_status_event);
  cmd_runner.QueueLeAsyncCommand(
      EmbossCommandPacket::New<pw::bluetooth::emboss::CommandHeaderView>(
          hci_spec::kLEReadRemoteFeatures),
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
      cb,
      /*wait=*/false);

  cmd_runner.QueueCommand(CommandPacket::New(hci_spec::kReadRemoteVersionInfo),
                          cb,
                          /*wait=*/true,
                          hci_spec::kReadRemoteVersionInfoCompleteEventCode);

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunUntilIdle();
  // Command 2 should wait on command 0 & command 1 complete events.
  EXPECT_FALSE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());
  // Completing the commands out of order shouldn't matter.
  test_device()->SendCommandChannelPacket(command1_cmpl_event);
  test_device()->SendCommandChannelPacket(command0_cmpl_event);

  EXPECT_CMD_PACKET_OUT(
      test_device(), command2, &command2_status_event, &command2_cmpl_event);
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(3, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
}

TEST_F(SequentialCommandRunnerTest, ExclusiveAsyncCommands) {
  auto command = bt::testing::EmptyCommandPacket(hci_spec::kRemoteNameRequest);
  auto command0_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kRemoteNameRequest, pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command0_cmpl_event =
      bt::testing::RemoteNameRequestCompletePacket(DeviceAddress());

  auto command1 =
      bt::testing::EmptyCommandPacket(hci_spec::kReadRemoteVersionInfo);
  auto command1_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kReadRemoteVersionInfo,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command1_cmpl_event =
      bt::testing::ReadRemoteVersionInfoCompletePacket(/*conn=*/0x0000);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_CMD_PACKET_OUT(test_device(), command, &command0_status_event);
  cmd_runner.QueueCommand(CommandPacket::New(hci_spec::kRemoteNameRequest),
                          cb,
                          /*wait=*/false,
                          hci_spec::kRemoteNameRequestCompleteEventCode);

  // Even though command 1 is not waiting on command 0, it should remain queued
  // due to the exclusion list.
  cmd_runner.QueueCommand(CommandPacket::New(hci_spec::kReadRemoteVersionInfo),
                          cb,
                          /*wait=*/false,
                          hci_spec::kReadRemoteVersionInfoCompleteEventCode,
                          /*exclusions=*/{hci_spec::kRemoteNameRequest});

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);

  RunUntilIdle();
  EXPECT_FALSE(cmd_runner.IsReady());
  // Command 1 is "sent" but queued in CommandChannel.
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  // Completing command 0 should send command 1.
  test_device()->SendCommandChannelPacket(command0_cmpl_event);

  EXPECT_CMD_PACKET_OUT(
      test_device(), command1, &command1_status_event, &command1_cmpl_event);
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
}

TEST_F(SequentialCommandRunnerTest,
       CommandRunnerDestroyedBeforeSecondEventCallbackCalled) {
  auto command = bt::testing::EmptyCommandPacket(hci_spec::kRemoteNameRequest);
  auto command0_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kRemoteNameRequest, pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command0_cmpl_event =
      bt::testing::RemoteNameRequestCompletePacket(DeviceAddress());

  auto command1 =
      bt::testing::EmptyCommandPacket(hci_spec::kLEReadRemoteFeatures);
  auto command1_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kLEReadRemoteFeatures,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command1_cmpl_event = bt::testing::LEReadRemoteFeaturesCompletePacket(
      /*conn=*/0x0000, hci_spec::LESupportedFeatures{0});

  std::optional<SequentialCommandRunner> cmd_runner;
  cmd_runner.emplace(cmd_channel()->AsWeakPtr());

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) {
    if (cb_called == 0) {
      cmd_runner.reset();
    }
    cb_called++;
  };

  EXPECT_FALSE(cmd_runner->HasQueuedCommands());

  EXPECT_CMD_PACKET_OUT(
      test_device(), command, &command0_status_event, &command0_cmpl_event);
  cmd_runner->QueueCommand(CommandPacket::New(hci_spec::kRemoteNameRequest),
                           cb,
                           /*wait=*/false,
                           hci_spec::kRemoteNameRequestCompleteEventCode);

  EXPECT_CMD_PACKET_OUT(test_device(), command1, &command1_status_event);
  cmd_runner->QueueLeAsyncCommand(
      CommandPacket::New(hci_spec::kLEReadRemoteFeatures),
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
      cb,
      /*wait=*/false);

  EXPECT_TRUE(cmd_runner->IsReady());
  EXPECT_TRUE(cmd_runner->HasQueuedCommands());

  cmd_runner->RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner->IsReady());

  RunUntilIdle();
  EXPECT_FALSE(cmd_runner.has_value());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(0, status_cb_called);
}

TEST_F(SequentialCommandRunnerTest,
       SequentialCommandRunnerDestroyedInCancelStatusCallbackDoesNotCrash) {
  std::optional<SequentialCommandRunner> cmd_runner;
  cmd_runner.emplace(cmd_channel()->AsWeakPtr());

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
    cmd_runner.reset();
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  auto command = bt::testing::EmptyCommandPacket(kTestOpCode);
  cmd_runner->QueueCommand(CommandPacket::New(kTestOpCode), cb);
  EXPECT_CMD_PACKET_OUT(test_device(), command);
  cmd_runner->RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner->IsReady());
  cmd_runner->Cancel();

  RunUntilIdle();
  EXPECT_FALSE(cmd_runner);
  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(HostError::kCanceled), status);
}

TEST_F(SequentialCommandRunnerTest, QueueCommandsWhileAlreadyRunning) {
  auto command = bt::testing::EmptyCommandPacket(hci_spec::kRemoteNameRequest);
  auto command0_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kRemoteNameRequest, pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command0_cmpl_event =
      bt::testing::RemoteNameRequestCompletePacket(DeviceAddress());

  auto command1 =
      bt::testing::EmptyCommandPacket(hci_spec::kLEReadRemoteFeatures);
  auto command1_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kLEReadRemoteFeatures,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command1_cmpl_event = bt::testing::LEReadRemoteFeaturesCompletePacket(
      /*conn=*/0x0000, hci_spec::LESupportedFeatures{0});

  auto command2 =
      bt::testing::EmptyCommandPacket(hci_spec::kReadRemoteVersionInfo);
  auto command2_status_event = bt::testing::CommandStatusPacket(
      hci_spec::kReadRemoteVersionInfo,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  auto command2_cmpl_event =
      bt::testing::ReadRemoteVersionInfoCompletePacket(/*conn=*/0x0000);

  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  int name_cb_called = 0;
  auto name_request_callback = [&](const EventPacket& event) {
    name_cb_called++;

    EXPECT_FALSE(cmd_runner.IsReady());
    EXPECT_FALSE(cmd_runner.HasQueuedCommands());

    EXPECT_CMD_PACKET_OUT(
        test_device(), command1, &command1_status_event, &command1_cmpl_event);
    cmd_runner.QueueLeAsyncCommand(
        CommandPacket::New(hci_spec::kLEReadRemoteFeatures),
        hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
        cb,
        /*wait=*/false);

    EXPECT_CMD_PACKET_OUT(
        test_device(), command2, &command2_status_event, &command2_cmpl_event);
    cmd_runner.QueueCommand(
        CommandPacket::New(hci_spec::kReadRemoteVersionInfo),
        cb,
        /*wait=*/false,
        hci_spec::kReadRemoteVersionInfoCompleteEventCode);
  };

  EXPECT_CMD_PACKET_OUT(
      test_device(), command, &command0_status_event, &command0_cmpl_event);
  cmd_runner.QueueCommand(CommandPacket::New(hci_spec::kRemoteNameRequest),
                          name_request_callback,
                          /*wait=*/false,
                          hci_spec::kRemoteNameRequestCompleteEventCode);
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, name_cb_called);
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fit::ok(), status);
}

TEST_F(SequentialCommandRunnerTest, EmbossEventHandler) {
  SequentialCommandRunner cmd_runner(cmd_channel()->AsWeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  // HCI command with custom opcode FFFF.
  StaticByteBuffer command_bytes(0xFF, 0xFF, 0x00);
  StaticByteBuffer command_cmpl_success_bytes(
      hci_spec::kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 byte payload)
      1,
      0xFF,
      0xFF,
      pw::bluetooth::emboss::StatusCode::SUCCESS);

  Result<> status = fit::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  SequentialCommandRunner::EmbossCommandCompleteCallback cb =
      [&](const EmbossEventPacket& event) {
        cb_called++;
        EXPECT_THAT(event.data(), BufferEq(command_cmpl_success_bytes));
      };

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), std::move(cb));
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  EXPECT_CMD_PACKET_OUT(
      test_device(), command_bytes, &command_cmpl_success_bytes);
  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), status);
}

}  // namespace
}  // namespace bt::hci
