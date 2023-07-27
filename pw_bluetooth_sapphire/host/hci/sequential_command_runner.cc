// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sequential_command_runner.h"

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"

namespace bt::hci {

SequentialCommandRunner::SequentialCommandRunner(hci::CommandChannel::WeakPtr cmd_channel)
    : cmd_(std::move(cmd_channel)),
      sequence_number_(0u),
      running_commands_(0u),
      weak_ptr_factory_(this) {
  BT_DEBUG_ASSERT(cmd_.is_alive());
}

void SequentialCommandRunner::QueueCommand(CommandPacketVariant command_packet,
                                           CommandCompleteCallbackVariant callback, bool wait,
                                           hci_spec::EventCode complete_event_code,
                                           std::unordered_set<hci_spec::OpCode> exclusions) {
  if (std::holds_alternative<std::unique_ptr<CommandPacket>>(command_packet)) {
    BT_DEBUG_ASSERT(sizeof(hci_spec::CommandHeader) <=
                    std::get<std::unique_ptr<CommandPacket>>(command_packet)->view().size());
  }

  command_queue_.emplace(QueuedCommand{.packet = std::move(command_packet),
                                       .complete_event_code = complete_event_code,
                                       .is_le_async_command = false,
                                       .callback = std::move(callback),
                                       .wait = wait,
                                       .exclusions = std::move(exclusions)});

  if (status_callback_) {
    TryRunNextQueuedCommand();
  }
}

void SequentialCommandRunner::QueueLeAsyncCommand(CommandPacketVariant command_packet,
                                                  hci_spec::EventCode le_meta_subevent_code,
                                                  CommandCompleteCallbackVariant callback,
                                                  bool wait) {
  command_queue_.emplace(QueuedCommand{.packet = std::move(command_packet),
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
  BT_DEBUG_ASSERT(!status_callback_);
  BT_DEBUG_ASSERT(status_callback);
  BT_DEBUG_ASSERT(!command_queue_.empty());

  status_callback_ = std::move(status_callback);
  sequence_number_++;

  TryRunNextQueuedCommand();
}

bool SequentialCommandRunner::IsReady() const { return !status_callback_; }

void SequentialCommandRunner::Cancel() { NotifyStatusAndReset(ToResult(HostError::kCanceled)); }

bool SequentialCommandRunner::HasQueuedCommands() const { return !command_queue_.empty(); }

void SequentialCommandRunner::TryRunNextQueuedCommand(Result<> status) {
  BT_DEBUG_ASSERT(status_callback_);

  // If an error occurred or we're done, reset.
  if (status.is_error() || (command_queue_.empty() && running_commands_ == 0)) {
    NotifyStatusAndReset(status);
    return;
  }

  // Wait for the rest of the running commands to finish if we need to.
  if (command_queue_.empty() || (running_commands_ > 0 && command_queue_.front().wait)) {
    return;
  }

  QueuedCommand next = std::move(command_queue_.front());
  command_queue_.pop();

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto command_callback = [self, cmd_cb = std::move(next.callback),
                           complete_event_code = next.complete_event_code,
                           seq_no = sequence_number_](auto, const EventPacket& event_packet) {
    std::optional<EmbossEventPacket> emboss_packet;
    hci::Result<> status = bt::ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS);
    using T = std::decay_t<decltype(cmd_cb)>;
    if constexpr (std::is_same_v<T, CommandCompleteCallback>) {
      status = event_packet.ToResult();
    } else {
      emboss_packet = EmbossEventPacket::New(event_packet.view().size());
      MutableBufferView buffer = emboss_packet->mutable_data();
      event_packet.view().data().Copy(&buffer);
      status = emboss_packet->ToResult();
    }

    if (self.is_alive() && seq_no != self->sequence_number_) {
      bt_log(TRACE, "hci", "Ignoring event for previous sequence (event code: %#.2x, status: %s)",
             event_packet.event_code(), bt_str(status));
    }

    // The sequence could have failed or been canceled, and a new sequence could have started.
    // This check prevents calling cmd_cb if the corresponding sequence is no longer running.
    if (!self.is_alive() || !self->status_callback_ || seq_no != self->sequence_number_) {
      return;
    }

    if (status.is_ok() && event_packet.event_code() == hci_spec::kCommandStatusEventCode &&
        complete_event_code != hci_spec::kCommandStatusEventCode) {
      return;
    }

    std::visit(
        [&event_packet, &emboss_packet](auto& cmd_cb) {
          using T = std::decay_t<decltype(cmd_cb)>;
          if constexpr (std::is_same_v<T, CommandCompleteCallback>) {
            if (cmd_cb) {
              cmd_cb(event_packet);
            }
          } else if constexpr (std::is_same_v<T, EmbossCommandCompleteCallback>) {
            if (cmd_cb) {
              cmd_cb(*emboss_packet);
            }
          }
        },
        cmd_cb);

    // The callback could have destroyed, canceled, or restarted the command runner.  While this
    // check looks redundant to the above check, the state could have changed in cmd_cb.
    if (!self.is_alive() || !self->status_callback_ || seq_no != self->sequence_number_) {
      return;
    }

    BT_DEBUG_ASSERT(self->running_commands_ > 0);
    self->running_commands_--;
    self->TryRunNextQueuedCommand(status);
  };

  running_commands_++;
  if (!SendQueuedCommand(std::move(next), std::move(command_callback))) {
    NotifyStatusAndReset(ToResult(HostError::kFailed));
  } else {
    TryRunNextQueuedCommand();
  }
}

bool SequentialCommandRunner::SendQueuedCommand(QueuedCommand command,
                                                CommandChannel::CommandCallback callback) {
  if (!cmd_.is_alive()) {
    bt_log(INFO, "hci", "SequentialCommandRunner command channel died, aborting");
    return false;
  }
  if (command.is_le_async_command) {
    return cmd_->SendLeAsyncCommand(std::move(command.packet), std::move(callback),
                                    command.complete_event_code);
  }

  return cmd_->SendExclusiveCommand(std::move(command.packet), std::move(callback),
                                    command.complete_event_code, std::move(command.exclusions));
}

void SequentialCommandRunner::Reset() {
  if (!command_queue_.empty()) {
    command_queue_ = {};
  }
  running_commands_ = 0;
  status_callback_ = nullptr;
}

void SequentialCommandRunner::NotifyStatusAndReset(Result<> status) {
  BT_DEBUG_ASSERT(status_callback_);
  auto status_cb = std::move(status_callback_);
  Reset();
  status_cb(status);
}

}  // namespace bt::hci
