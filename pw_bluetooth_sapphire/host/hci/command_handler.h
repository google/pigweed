// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_HANDLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_HANDLER_H_

#include <lib/fit/defer.h>
#include <lib/fpromise/result.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::hci {

// CommandHandler is a wrapper around CommandChannel that abstracts serializing & deserializing of
// command and event packets. Command and event types must implement methods and fields documented
// above each method.
// TODO(fxbug.dev/57720): Types should match PDL generated packet definitions.
//
// This class does not track state regarding commands and events, so it may be used as either a
// temporary or saved object.
class CommandHandler {
 public:
  explicit CommandHandler(fxl::WeakPtr<CommandChannel> channel) : channel_(channel) {}

  // Wrapper around CommandChannel::SendCommand that sends a CommandT and completes on
  // CommandT::EventT.
  //
  // If an event status field indicates an error, that error will be returned instead of the event.
  //
  // The status event of async commands will be ignored unless it is an error.
  //
  // CommandT must implement:
  // std::unique_ptr<CommandPacket> Encode();
  // using EventT = ...;
  // static OpCode opcode();
  //
  // EventT must implement:
  // static fpromise::result<EventT, HostError> Decode(const EventPacket& packet);
  // static constexpr uint8_t kEventCode = ...;
  template <typename CommandT>
  CommandChannel::TransactionId SendCommand(
      CommandT command,
      fit::callback<void(fpromise::result<typename CommandT::EventT, Status>)> event_cb) {
    // EventT should be the command complete event code. Use SendCommandFinishOnStatus to only
    // handle the command status event.
    static_assert(CommandT::EventT::kEventCode != kCommandStatusEventCode);
    ZX_ASSERT(event_cb);

    auto encoded = command.Encode();
    auto event_packet_cb = [event_cb = std::move(event_cb)](
                               auto id, const EventPacket& event_packet) mutable {
      ZX_ASSERT_MSG(event_cb, "SendCommand event callback already called (opcode: %#.4x)",
                    CommandT::opcode());

      auto status = event_packet.ToStatus();
      if (!status.is_success()) {
        event_cb(fpromise::error(status));
        return;
      }

      // Ignore success status event if it is not the expected completion event.
      if (event_packet.event_code() == hci::kCommandStatusEventCode &&
          CommandT::EventT::kEventCode != hci::kCommandStatusEventCode) {
        bt_log(TRACE, "hci", "received success command status event (opcode: %#.4x)",
               CommandT::opcode());
        return;
      }

      ZX_ASSERT(event_packet.event_code() == CommandT::EventT::kEventCode);

      auto event_result = CommandT::EventT::Decode(event_packet);
      if (event_result.is_error()) {
        bt_log(WARN, "hci", "Error decoding event packet (event: %#.2x, error: %s)",
               event_packet.event_code(), HostErrorToString(event_result.error()).c_str());
        event_cb(fpromise::error(hci::Status(event_result.error())));
        return;
      }

      event_cb(fpromise::ok(event_result.take_value()));
    };
    return channel_->SendCommand(std::move(encoded), std::move(event_packet_cb),
                                 CommandT::EventT::kEventCode);
  }

  // Same as SendCommand, but completes on the command status event.
  // The complete event WILL BE IGNORED if no event handler is registered.
  //
  // This is useful when the command complete event is already handled by an event handler, and you
  // only need to handle command errors.
  //
  // Example:
  // handler.AddEventHandler(fit::bind_member(this, &BrEdrConnectionManager::OnConnectionComplete));
  //
  // handler.SendCommandFinishOnStatus(
  //  CreateConnectionCommand{...},
  //  [](auto result) {
  //    if (result.is_error()) {
  //      // Handle error
  //      return;
  //    }
  // });
  template <typename CommandT>
  CommandChannel::TransactionId SendCommandFinishOnStatus(
      CommandT command, fit::callback<void(fpromise::result<void, Status>)> status_cb) {
    ZX_ASSERT(status_cb);

    auto encoded = command.Encode();
    auto event_packet_cb = [status_cb = std::move(status_cb)](
                               auto id, const EventPacket& event_packet) mutable {
      ZX_ASSERT(event_packet.event_code() == hci::kCommandStatusEventCode);

      auto status = event_packet.ToStatus();
      if (!status.is_success()) {
        status_cb(fpromise::error(status));
        return;
      }

      status_cb(fpromise::ok());
    };
    return channel_->SendCommand(std::move(encoded), std::move(event_packet_cb),
                                 hci::kCommandStatusEventCode);
  }

  // Wrapper around CommandChannel::AddEventHandler that calls |handler| with an EventT.
  //
  // EventT must implement:
  // static fpromise::result<EventT, HostError> Decode(const EventPacket& packet);
  // static constexpr uint8_t kEventCode = ...;
  template <typename EventT>
  CommandChannel::EventHandlerId AddEventHandler(
      fit::function<CommandChannel::EventCallbackResult(EventT)> handler) {
    ZX_ASSERT(handler);

    auto event_packet_cb = [handler = std::move(handler)](const EventPacket& event_packet) {
      auto event_result = EventT::Decode(event_packet);
      if (event_result.is_error()) {
        bt_log(WARN, "hci", "Error decoding event packet (event: %#.2x, error: %s)",
               event_packet.event_code(), HostErrorToString(event_result.error()).c_str());
        return CommandChannel::EventCallbackResult::kContinue;
      }
      return handler(event_result.take_value());
    };
    return channel_->AddEventHandler(EventT::kEventCode, std::move(event_packet_cb));
  }

 private:
  fxl::WeakPtr<CommandChannel> channel_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_COMMAND_HANDLER_H_
