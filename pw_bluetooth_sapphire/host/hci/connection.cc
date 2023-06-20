// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include <endian.h>

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

#include <pw_bluetooth/hci.emb.h>

namespace bt::hci {

Connection::Connection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                       const DeviceAddress& peer_address, const Transport::WeakPtr& hci,
                       fit::callback<void()> on_disconnection_complete)
    : handle_(handle),
      local_address_(local_address),
      peer_address_(peer_address),
      conn_state_(State::kConnected),
      hci_(hci),
      weak_self_(this) {
  BT_ASSERT(hci_.is_alive());

  auto disconn_complete_handler =
      [self = weak_self_.GetWeakPtr(), handle, hci = hci_,
       on_disconnection_complete =
           std::move(on_disconnection_complete)](const EmbossEventPacket& event) mutable {
        return Connection::OnDisconnectionComplete(self, handle, event,
                                                   std::move(on_disconnection_complete));
      };
  hci_->command_channel()->AddEventHandler(hci_spec::kDisconnectionCompleteEventCode,
                                           std::move(disconn_complete_handler));
}

Connection::~Connection() {
  if (conn_state_ == Connection::State::kConnected) {
    Disconnect(pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
  }
}

std::string Connection::ToString() const {
  return bt_lib_cpp_string::StringPrintf("[HCI connection (handle: %#.4x, address: %s)]", handle_,
                                         bt_str(peer_address_));
}

CommandChannel::EventCallbackResult Connection::OnDisconnectionComplete(
    const WeakSelf<Connection>::WeakPtr& self, hci_spec::ConnectionHandle handle,
    const EmbossEventPacket& event, fit::callback<void()> on_disconnection_complete) {
  BT_ASSERT(event.event_code() == hci_spec::kDisconnectionCompleteEventCode);

  auto view = event.view<pw::bluetooth::emboss::DisconnectionCompleteEventView>();
  if (!view.Ok()) {
    bt_log(WARN, "hci", "malformed disconnection complete event");
    return CommandChannel::EventCallbackResult::kContinue;
  }

  const hci_spec::ConnectionHandle event_handle = view.connection_handle().Read();

  // Silently ignore this event as it isn't meant for this connection.
  if (event_handle != handle) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(INFO, "hci", "disconnection complete - %s, handle: %#.4x, reason: %#.2hhx (%s)",
         bt_str(event.ToResult()), handle, static_cast<unsigned char>(view.reason().Read()),
         hci_spec::StatusCodeToString(view.reason().Read()).c_str());

  if (self.is_alive()) {
    self->conn_state_ = State::kDisconnected;
  }

  // Peer disconnect. Callback may destroy connection.
  if (self.is_alive() && self->peer_disconnect_callback_) {
    self->peer_disconnect_callback_(self.get(), view.reason().Read());
  }

  // Notify subclasses after peer_disconnect_callback_ has had a chance to clean up higher-level
  // connections.
  if (on_disconnection_complete) {
    on_disconnection_complete();
  }

  return CommandChannel::EventCallbackResult::kRemove;
}

void Connection::Disconnect(pw::bluetooth::emboss::StatusCode reason) {
  BT_ASSERT(conn_state_ == Connection::State::kConnected);

  conn_state_ = Connection::State::kWaitingForDisconnectionComplete;

  // Here we send a HCI_Disconnect command without waiting for it to complete.
  auto status_cb = [](auto id, const EventPacket& event) {
    BT_DEBUG_ASSERT(event.event_code() == hci_spec::kCommandStatusEventCode);
    hci_is_error(event, TRACE, "hci", "ignoring disconnection failure");
  };

  auto disconn = EmbossCommandPacket::New<pw::bluetooth::emboss::DisconnectCommandWriter>(
      hci_spec::kDisconnect);
  auto params = disconn.view_t();
  params.connection_handle().Write(handle());
  params.reason().Write(reason);

  bt_log(DEBUG, "hci", "disconnecting connection (handle: %#.4x, reason: %#.2hhx)", handle(),
         static_cast<unsigned char>(reason));

  // Send HCI Disconnect.
  hci_->command_channel()->SendCommand(std::move(disconn), std::move(status_cb),
                                       hci_spec::kCommandStatusEventCode);
}

}  // namespace bt::hci
