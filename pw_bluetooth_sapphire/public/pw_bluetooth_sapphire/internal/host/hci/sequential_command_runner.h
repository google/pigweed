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

#pragma once
#include <queue>

#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

namespace bt::hci {

class Transport;

// A SequentialCommandRunner can be used to chain HCI commands such that
// commands in the sequence are sent to the controller only after previous
// commands have completed successfully.
//
// When a command fails due to an error status (in a HCI_Command_Status or
// HCI_Command_Complete event), the rest of the sequence is abandoned and an
// error status is reported back to the caller. Already sent commands will
// continue but not report their statuses to the caller.
//
// Commands are always sent in the order that they are queued.  If any command
// fails, unsent commands are abandoned.
//
// Parts of the sequence can be run in parallel by using the |wait| parameter
// of QueueCommand.  If |wait| is true, all commands queued before that command
// must complete (and succeed) before the command will be sent.
//
// For example:
// QueueCommand(a, [](const auto&){}, true);
// QueueCommand(b, [](const auto&){}, false);
// QueueCommand(c, [](const auto&){}, false);
// QueueCommand(d, [](const auto&){}, true);
// QueueCommand(e, [](const auto&){}, false);
// RunCommands([](auto){});
//
// Command a, b, and c will be run simultaneously.  If any failed, then
// RunCommands will report that error back to the caller.  All three of a, b,
// and c's callbacks are run.  If all three succeeded, then d and e will run
// simultaneously, and when both complete, the whole sequence is complete and
// RunCommands will report success.
class SequentialCommandRunner final {
 public:
  using CommandPacketVariant = CommandChannel::CommandPacketVariant;

  explicit SequentialCommandRunner(hci::CommandChannel::WeakPtr cmd_channel);
  ~SequentialCommandRunner() = default;

  // Adds a HCI command packet to the queue.
  // If |callback| is provided, it will run with the event that completes the
  // command.
  // If |wait| is true, then all previously queued commands must complete
  // successfully before this command is sent.
  // |exclusions| will be passed to CommandChannel::SendExclusiveCommand().
  using CommandCompleteCallback = fit::function<void(const EventPacket& event)>;
  using EmbossCommandCompleteCallback =
      fit::function<void(const EmbossEventPacket& event_packet)>;
  using CommandCompleteCallbackVariant =
      std::variant<CommandCompleteCallback, EmbossCommandCompleteCallback>;
  void QueueCommand(
      CommandPacketVariant command_packet,
      CommandCompleteCallbackVariant callback = CommandCompleteCallback(),
      bool wait = true,
      hci_spec::EventCode complete_event_code =
          hci_spec::kCommandCompleteEventCode,
      std::unordered_set<hci_spec::OpCode> exclusions = {});

  // Same as QueueCommand(), except the command completes on the LE Meta Event
  // with subevent code |le_meta_subevent_code|.
  void QueueLeAsyncCommand(
      CommandPacketVariant command_packet,
      hci_spec::EventCode le_meta_subevent_code,
      CommandCompleteCallbackVariant callback = CommandCompleteCallback(),
      bool wait = true);

  // Runs all the queued commands. This method will return before queued
  // commands have been run. |status_callback| is called with the status of the
  // last command run, or kSuccess if all commands returned
  // HCI_Command_Complete.
  //
  // Once RunCommands() has been called this instance will not be ready for
  // re-use until |status_callback| gets run. At that point new commands can be
  // queued and run (see IsReady()).
  //
  // Commands can continue to be queued while commands are being run, including
  // from inside of command callbacks.
  //
  // RunCommands() will always send at least one HCI command to CommandChannel
  // if any are queued, which can not be prevented by a call to Cancel().
  void RunCommands(ResultFunction<> status_callback);

  // Returns true if commands are not running on this instance. This
  // returns false if RunCommands() is currently in progress.
  bool IsReady() const;

  // Cancels a running sequence. RunCommands() must have been called before a
  // sequence can be cancelled. Once a sequence is cancelled, the state of the
  // SequentialCommandRunner will be reset (i.e. IsReady() will return true).
  // The result of any running HCI command will not be reported to the
  // corresponding command callback and the result callback will be called
  // with HostError::kCanceled.
  //
  // Depending on the sequence of HCI commands that were previously processed,
  // the controller will be in an undefined state. The caller is responsible for
  // sending successive HCI commands to bring the controller back to an expected
  // state.
  //
  // After a call to Cancel(), this object can be immediately reused to queue up
  // a new HCI command sequence.
  void Cancel();

  // Returns true if this currently has any pending commands.
  bool HasQueuedCommands() const;

 private:
  struct QueuedCommand {
    CommandPacketVariant packet;
    hci_spec::EventCode complete_event_code;
    bool is_le_async_command;
    CommandCompleteCallbackVariant callback;
    bool wait;
    std::unordered_set<hci_spec::OpCode> exclusions;
  };

  // Try to run the next queued command.
  // |status| is the result of the most recently completed command.
  // Aborts the sequence with |status| if it did not succeed.
  // Completes the sequence with |status| no commands are running or queued.
  // Runs the next queued command if it doesn't wait for the previous commands.
  // Runs the next queued command if no commands are running.
  void TryRunNextQueuedCommand(Result<> status = fit::ok());

  // Returns true on success, false on failure.
  bool SendQueuedCommand(QueuedCommand command,
                         CommandChannel::CommandCallback callback);

  void Reset();
  void NotifyStatusAndReset(Result<> status);

  hci::CommandChannel::WeakPtr cmd_;

  std::queue<QueuedCommand> command_queue_;

  // Callback assigned in RunCommands(). If this is non-null then this object is
  // currently executing a sequence.
  ResultFunction<> status_callback_;

  // Number assigned to the current sequence. Each "sequence" begins on a call
  // to RunCommands() and ends either on a call to Cancel() or when
  // |status_callback_| has been invoked.
  //
  // This number is used to detect cancelation of a sequence from a
  // CommandCompleteCallback.
  uint64_t sequence_number_;

  // Number of commands sent to the controller we are waiting to finish.
  size_t running_commands_;

  WeakSelf<SequentialCommandRunner> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SequentialCommandRunner);
};

}  // namespace bt::hci
