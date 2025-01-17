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

#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"

#include <pw_assert/check.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::hci {

SequentialCommandRunner::SequentialCommandRunner(
    hci::CommandChannel::WeakPtr cmd_channel)
    : cmd_(std::move(cmd_channel)),
      sequence_number_(0u),
      running_commands_(0u),
      weak_ptr_factory_(this) {
  PW_DCHECK(cmd_.is_alive());
}

void SequentialCommandRunner::QueueCommand(
    CommandPacket command_packet,
    EmbossCommandCompleteCallback callback,
    bool wait,
    hci_spec::EventCode complete_event_code,
    std::unordered_set<hci_spec::OpCode> exclusions) {
  command_queue_.emplace(
      QueuedCommand{.packet = std::move(command_packet),
                    .complete_event_code = complete_event_code,
                    .is_le_async_command = false,
                    .callback = std::move(callback),
                    .wait = wait,
                    .exclusions = std::move(exclusions)});

  if (status_callback_) {
    TryRunNextQueuedCommand();
  }
}

void SequentialCommandRunner::QueueLeAsyncCommand(
    CommandPacket command_packet,
    hci_spec::EventCode le_meta_subevent_code,
    EmbossCommandCompleteCallback callback,
    bool wait) {
  command_queue_.emplace(
      QueuedCommand{.packet = std::move(command_packet),
                    .complete_event_code = le_meta_subevent_code,
                    .is_le_async_command = true,
                    .callback = std::move(callback),
                    .wait = wait,
                    .exclusions = {}});

  if (status_callback_) {
    TryRunNextQueuedCommand();
  }
}

void SequentialCommandRunner::RunCommands(ResultFunction<> status_callback) {
  PW_DCHECK(!status_callback_);
  PW_DCHECK(status_callback);
  PW_DCHECK(!command_queue_.empty());

  status_callback_ = std::move(status_callback);
  sequence_number_++;

  TryRunNextQueuedCommand();
}

bool SequentialCommandRunner::IsReady() const { return !status_callback_; }

void SequentialCommandRunner::Cancel() {
  NotifyStatusAndReset(ToResult(HostError::kCanceled));
}

bool SequentialCommandRunner::HasQueuedCommands() const {
  return !command_queue_.empty();
}

void SequentialCommandRunner::TryRunNextQueuedCommand(Result<> status) {
  PW_DCHECK(status_callback_);

  // If an error occurred or we're done, reset.
  if (status.is_error() || (command_queue_.empty() && running_commands_ == 0)) {
    NotifyStatusAndReset(status);
    return;
  }

  // Wait for the rest of the running commands to finish if we need to.
  if (command_queue_.empty() ||
      (running_commands_ > 0 && command_queue_.front().wait)) {
    return;
  }

  QueuedCommand next = std::move(command_queue_.front());
  command_queue_.pop();

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto command_callback = [self,
                           cmd_cb = std::move(next.callback),
                           complete_event_code = next.complete_event_code,
                           seq_no = sequence_number_](
                              auto, const EventPacket& event) {
    hci::Result<> event_result = event.ToResult();

    if (self.is_alive() && seq_no != self->sequence_number_) {
      bt_log(TRACE,
             "hci",
             "Ignoring event for previous sequence (event code: %#.2x, status: "
             "%s)",
             event.event_code(),
             bt_str(event_result));
    }

    // The sequence could have failed or been canceled, and a new sequence could
    // have started. This check prevents calling cmd_cb if the corresponding
    // sequence is no longer running.
    if (!self.is_alive() || !self->status_callback_ ||
        seq_no != self->sequence_number_) {
      return;
    }

    if (event_result.is_ok() &&
        event.event_code() == hci_spec::kCommandStatusEventCode &&
        complete_event_code != hci_spec::kCommandStatusEventCode) {
      return;
    }

    if (cmd_cb) {
      cmd_cb(event);
    }

    // The callback could have destroyed, canceled, or restarted the command
    // runner.  While this check looks redundant to the above check, the state
    // could have changed in cmd_cb.
    if (!self.is_alive() || !self->status_callback_ ||
        seq_no != self->sequence_number_) {
      return;
    }

    PW_DCHECK(self->running_commands_ > 0);
    self->running_commands_--;
    self->TryRunNextQueuedCommand(event_result);
  };

  running_commands_++;
  if (!SendQueuedCommand(std::move(next), std::move(command_callback))) {
    NotifyStatusAndReset(ToResult(HostError::kFailed));
  } else {
    TryRunNextQueuedCommand();
  }
}

bool SequentialCommandRunner::SendQueuedCommand(
    QueuedCommand command, CommandChannel::CommandCallback callback) {
  if (!cmd_.is_alive()) {
    bt_log(
        INFO, "hci", "SequentialCommandRunner command channel died, aborting");
    return false;
  }
  if (command.is_le_async_command) {
    return cmd_->SendLeAsyncCommand(std::move(command.packet),
                                    std::move(callback),
                                    command.complete_event_code);
  }

  return cmd_->SendExclusiveCommand(std::move(command.packet),
                                    std::move(callback),
                                    command.complete_event_code,
                                    std::move(command.exclusions));
}

void SequentialCommandRunner::Reset() {
  if (!command_queue_.empty()) {
    command_queue_ = {};
  }
  running_commands_ = 0;
  status_callback_ = nullptr;
}

void SequentialCommandRunner::NotifyStatusAndReset(Result<> status) {
  PW_DCHECK(status_callback_);
  auto status_cb = std::move(status_callback_);
  Reset();
  status_cb(status);
}

}  // namespace bt::hci
