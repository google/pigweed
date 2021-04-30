// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SEQUENTIAL_COMMAND_RUNNER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SEQUENTIAL_COMMAND_RUNNER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/thread_checker.h>

#include <queue>

#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/status.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/memory/ref_ptr.h"

namespace bt::hci {

class Transport;

// A SequentialCommandRunner can be used to chain HCI commands such that
// commands in the sequence are sent to the controller only after previous
// commands have completed successfully.
//
// When a command fails due to an error status (in a HCI_Command_Status or
// HCI_Command_Complete event), the rest of the sequence is abandoned and an
// error status is reported back to the caller. Already sent commands will
// continue and report their statuses back to their individual callbacks.
//
// Only commands that terminate with the HCI_Command_Complete event are
// currently supported.
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
//
// This class is not thread-safe. The dispatcher that is provided during
// initialization must be bound to the thread on which the instance of
// SequentialCommandRunner is being constructed.
class SequentialCommandRunner final {
 public:
  SequentialCommandRunner(async_dispatcher_t* dispatcher, fxl::WeakPtr<Transport> transport);
  ~SequentialCommandRunner();

  // Adds a HCI command packet to the queue.
  // If |callback| is provided, it will run with the event that completes the
  // command.
  // If |wait| is true, then all previously queued commands must complete
  // successfully before this command is sent.
  //
  // Cannot be called once RunCommands() has been called.
  using CommandCompleteCallback = fit::function<void(const EventPacket& event)>;
  void QueueCommand(std::unique_ptr<CommandPacket> command_packet,
                    CommandCompleteCallback callback = {}, bool wait = true);

  // Runs all the queued commands. Once this is called no new commands can be
  // queued. This method will return before queued commands have been run.
  // |status_callback| is called with the status of the last command run,
  // or kSuccess if all commands returned HCI_Command_Complete.
  //
  // Once RunCommands() has been called this instance will not be ready for
  // re-use until |status_callback| gets run. At that point new commands can be
  // queued and run (see IsReady()).
  //
  // RunCommands() will always send at least one HCI command to CommandChannel
  // if any are queued, which can not be prevented by a call to Cancel().
  using StatusCallback = fit::function<void(Status status)>;
  void RunCommands(StatusCallback status_callback);

  // Returns true if commands can be queued and run on this instance. This
  // returns false if RunCommands() is currently in progress.
  bool IsReady() const;

  // Cancels a running sequence. RunCommands() must have been called before a
  // sequence can be cancelled. Once a sequence is cancelled, the state of the
  // SequentialCommandRunner will be reset (i.e. IsReady() will return true).
  // The result of any running HCI command will still be reported to the
  // corresponding command callback.
  // and the result callback will be called with HostError::kCanceled.
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
  // Try to run the next queued command.
  // |status| is the result of the most recently completed command.
  // Aborts the sequence with |status| if it did not succeed.
  // Completes the sequence with |status| no commands are running or queued.
  // Runs the next queued command if it doesn't wait for the previous commands.
  // Runs the next queued command if no commands are running.
  void TryRunNextQueuedCommand(Status status = Status());
  void Reset();
  void NotifyStatusAndReset(Status status);

  async_dispatcher_t* dispatcher_;
  fxl::WeakPtr<Transport> transport_;

  struct QueuedCommand {
    std::unique_ptr<CommandPacket> packet;
    CommandCompleteCallback callback;
    bool wait;
  };

  std::queue<QueuedCommand> command_queue_;

  // Callback assigned in RunCommands(). If this is non-null then this object is
  // currently executing a sequence.
  StatusCallback status_callback_;

  // Number assigned to the current sequence. Each "sequence" begins on a call
  // to RunCommands() and ends either on a call to Cancel() or when
  // |status_callback_| has been invoked.
  //
  // This number is used to detect cancelation of a sequence from a
  // CommandCompleteCallback.
  uint64_t sequence_number_;

  // Number of commands sent to the controller we are waiting to finish.
  size_t running_commands_;

  fit::thread_checker thread_checker_;

  fxl::WeakPtrFactory<SequentialCommandRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SequentialCommandRunner);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SEQUENTIAL_COMMAND_RUNNER_H_
