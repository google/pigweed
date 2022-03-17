// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::hci {
namespace {

constexpr hci_spec::OpCode kTestOpCode = 0xFFFF;
constexpr hci_spec::OpCode kTestOpCode2 = 0xF00F;

using bt::testing::CommandTransaction;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class SequentialCommandRunnerTest : public TestingBase {
 public:
  SequentialCommandRunnerTest() = default;
  ~SequentialCommandRunnerTest() override = default;
};

using HCI_SequentialCommandRunnerTest = SequentialCommandRunnerTest;

TEST_F(SequentialCommandRunnerTest, SequentialCommandRunner) {
  // HCI command with custom opcode FFFF.
  auto command_bytes = CreateStaticByteBuffer(0xFF, 0xFF, 0x00);

  auto command_status_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandStatusEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             hci_spec::StatusCode::kHardwareFailure, 1, 0xFF, 0xFF);

  auto command_cmpl_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             1, 0xFF, 0xFF, hci_spec::StatusCode::kReserved0);

  auto command_cmpl_success_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             1, 0xFF, 0xFF, hci_spec::StatusCode::kSuccess);

  // Here we perform multiple test sequences where we queue up several  commands
  // in each sequence. We expect each sequence to terminate differently after
  // the following HCI transactions:
  //
  // Sequence 1 (HCI packets)
  //    -> Command; <- error status
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_status_error_bytes);

  // Sequence 2 (HCI packets)
  //    -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_error_bytes);

  // Sequence 3 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_error_bytes);

  // Sequence 4 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 5 (HCI packets)
  //    -> Command; <- success complete
  //    -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);

  test_device()->StartCmdChannel(test_cmd_chan());
  test_device()->StartAclChannel(test_acl_chan());

  Result<> status = fitx::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  // Sequence 1 (test)
  SequentialCommandRunner cmd_runner(dispatcher(), transport()->WeakPtr());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kHardwareFailure), status);
  cb_called = 0;

  // Sequence 2 (test)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(2, status_cb_called);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kReserved0), status);
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
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(3, status_cb_called);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kReserved0), status);
  cb_called = 0;

  // Sequence 4 (test)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(4, status_cb_called);
  EXPECT_EQ(fitx::ok(), status);
  cb_called = 0;
  status_cb_called = 0;

  // Sequence 5 (test) (no callback passed to QueueCommand)
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode));
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode));

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());
  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(fitx::ok(), status);
}

TEST_F(SequentialCommandRunnerTest, SequentialCommandRunnerCancel) {
  auto command_bytes = CreateStaticByteBuffer(0xFF, 0xFF, 0x00);

  auto command_cmpl_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             1, 0xFF, 0xFF, hci_spec::StatusCode::kHardwareFailure);

  auto command_cmpl_success_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             1, 0xFF, 0xFF, hci_spec::StatusCode::kSuccess);

  // Sequence 1
  //   -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 2
  //   -> Command; <- success complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);

  // Sequence 3
  //   -> Command; <- success complete
  //   -> Command; <- error complete
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_error_bytes);

  test_device()->StartCmdChannel(test_cmd_chan());
  test_device()->StartAclChannel(test_acl_chan());

  Result<> status = fitx::ok();
  int status_cb_called = 0;
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  int cb_called = 0;
  auto cb = [&](const EventPacket& event) { cb_called++; };

  // Sequence 1: Sequence will be cancelled after the first command.
  SequentialCommandRunner cmd_runner(dispatcher(), transport()->WeakPtr());
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // <-- Should not run
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_TRUE(cmd_runner.HasQueuedCommands());

  // Call RunCommands() and right away post a task to cancel the sequence. The
  // first command will go out but no successive packets should be sent.
  // status callbacks should be invoked
  // the command callback for the first command should run but no others.
  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());
  cmd_runner.Cancel();

  // Since |status_cb| is expected to not get called (which would normally quit
  // the message loop) - we run until we reach a steady-state waiting.
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_EQ(1, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(HostError::kCanceled), status);
  cb_called = 0;
  status_cb_called = 0;
  status = fitx::ok();

  // Sequence 2: Sequence will be cancelled after first command. This tests
  // canceling a sequence from a CommandCompleteCallback.
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), [&](const EventPacket& event) {
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
  RunLoopUntilIdle();
  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  EXPECT_EQ(0, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(HostError::kCanceled), status);

  // Sequence 3: Sequence will be cancelled after first command and immediately
  // followed by a second command which will fail. This tests canceling a
  // sequence and initiating a new one from a CommandCompleteCallback.
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), [&](const EventPacket& event) {
    cmd_runner.Cancel();
    EXPECT_TRUE(cmd_runner.IsReady());
    EXPECT_FALSE(cmd_runner.HasQueuedCommands());

    EXPECT_EQ(2, status_cb_called);
    EXPECT_EQ(ToResult(HostError::kCanceled), status);

    // Queue multiple commands (only one will execute since MockController
    // will send back an error status.
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

  RunLoopUntilIdle();

  EXPECT_TRUE(cmd_runner.IsReady());
  EXPECT_FALSE(cmd_runner.HasQueuedCommands());

  // The cb queued from inside the first callback should have been called.
  EXPECT_EQ(1, cb_called);
  // The result callback should have been called with the failure result.
  EXPECT_EQ(3, status_cb_called);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kHardwareFailure), status);
}

TEST_F(SequentialCommandRunnerTest, ParallelCommands) {
  // Need to signal to the queue that we can run more than one command at once.
  auto command_status_queue_increase =
      CreateStaticByteBuffer(hci_spec::kCommandStatusEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             hci_spec::StatusCode::kSuccess, 250, 0x00, 0x00);
  // HCI command with custom opcode FFFF.
  auto command_bytes = CreateStaticByteBuffer(0xFF, 0xFF, 0x00);
  auto command_status_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandStatusEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             hci_spec::StatusCode::kHardwareFailure, 2, 0xFF, 0xFF);

  auto command_cmpl_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             2, 0xFF, 0xFF, hci_spec::StatusCode::kReserved0);

  auto command_cmpl_success_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             2, 0xFF, 0xFF, hci_spec::StatusCode::kSuccess);

  // HCI command with custom opcode F00F.
  auto command2_bytes = CreateStaticByteBuffer(0x0F, 0xF0, 0x00);
  auto command2_status_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandStatusEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             hci_spec::StatusCode::kHardwareFailure, 2, 0x0F, 0xF0);

  auto command2_cmpl_error_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             2, 0x0F, 0xF0, hci_spec::StatusCode::kReserved0);

  auto command2_cmpl_success_bytes =
      CreateStaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                             0x04,  // parameter_total_size (4 byte payload)
                             2, 0x0F, 0xF0, hci_spec::StatusCode::kSuccess);

  test_device()->StartCmdChannel(test_cmd_chan());
  test_device()->StartAclChannel(test_acl_chan());
  test_device()->SendCommandChannelPacket(command_status_queue_increase);

  // Parallel commands should all run before commands that require success.
  // command and command2 are answered in opposite order because they should be
  // sent simultaneously.
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, );
  EXPECT_CMD_PACKET_OUT(test_device(), command2_bytes, );
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, &command_cmpl_success_bytes);

  int cb_called = 0;
  auto cb = [&](const auto&) { cb_called++; };

  int status_cb_called = 0;
  Result<> status = ToResult(HostError::kFailed);
  auto status_cb = [&](Result<> cb_status) {
    status = cb_status;
    status_cb_called++;
  };

  SequentialCommandRunner cmd_runner(dispatcher(), transport()->WeakPtr());

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode2), cb, /*wait=*/false);
  cmd_runner.QueueCommand(
      CommandPacket::New(kTestOpCode),
      [&](const auto&) {
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

  RunLoopUntilIdle();
  // The first two commands should have been sent but no responses are back yet.
  EXPECT_EQ(0, cb_called);

  // It should not matter if they get answered in opposite order.
  test_device()->SendCommandChannelPacket(command2_cmpl_success_bytes);
  test_device()->SendCommandChannelPacket(command_cmpl_success_bytes);
  RunLoopUntilIdle();

  EXPECT_EQ(5, cb_called);
  EXPECT_EQ(fitx::ok(), status);
  EXPECT_EQ(1, status_cb_called);
  cb_called = 0;
  status_cb_called = 0;

  // If any simultaneous commands fail, the sequence fails and the command
  // sequence is terminated.
  EXPECT_CMD_PACKET_OUT(test_device(), command_bytes, );
  EXPECT_CMD_PACKET_OUT(test_device(), command2_bytes, );

  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode), cb, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode2), cb, /*wait=*/false);
  cmd_runner.QueueCommand(CommandPacket::New(kTestOpCode),
                          cb);  // shouldn't run

  cmd_runner.RunCommands(status_cb);
  EXPECT_FALSE(cmd_runner.IsReady());

  RunLoopUntilIdle();
  // The first two commands should have been sent but no responses are back yet.
  EXPECT_EQ(0, cb_called);

  test_device()->SendCommandChannelPacket(command_status_error_bytes);
  test_device()->SendCommandChannelPacket(command2_cmpl_success_bytes);
  RunLoopUntilIdle();

  EXPECT_EQ(2, cb_called);
  EXPECT_EQ(1, status_cb_called);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kHardwareFailure), status);
}

}  // namespace
}  // namespace bt::hci
