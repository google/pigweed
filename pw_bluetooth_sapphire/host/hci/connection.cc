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

#include <src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h>

namespace bt::hci {

Connection::Connection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                       const DeviceAddress& peer_address, const Transport::WeakPtr& hci,
                       fit::callback<void()> on_disconnection_complete)
    : handle_(handle),
      local_address_(local_address),
      peer_address_(peer_address),
      conn_state_(State::kConnected),
      hci_(hci),
      weak_ptr_factory_(this) {
  BT_ASSERT(handle_);
  BT_ASSERT(hci_.is_alive());

  auto disconn_complete_handler = [self = weak_ptr_factory_.GetWeakPtr(), handle, hci = hci_,
                                   on_disconnection_complete =
                                       std::move(on_disconnection_complete)](auto& event) mutable {
    return Connection::OnDisconnectionComplete(self, handle, event,
                                               std::move(on_disconnection_complete));
  };
  hci_->command_channel()->AddEventHandler(hci_spec::kDisconnectionCompleteEventCode,
                                           std::move(disconn_complete_handler));
}

Connection::~Connection() {
  if (conn_state_ == Connection::State::kConnected) {
    Disconnect(hci_spec::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
  }
}

std::string Connection::ToString() const {
  return bt_lib_cpp_string::StringPrintf("[HCI connection (handle: %#.4x, address: %s)]", handle_,
                                         bt_str(peer_address_));
}

CommandChannel::EventCallbackResult Connection::OnDisconnectionComplete(
    fxl::WeakPtr<Connection> self, hci_spec::ConnectionHandle handle, const EventPacket& event,
    fit::callback<void()> on_disconnection_complete) {
  BT_ASSERT(event.event_code() == hci_spec::kDisconnectionCompleteEventCode);

  if (event.view().payload_size() != sizeof(hci_spec::DisconnectionCompleteEventParams)) {
    bt_log(WARN, "hci", "malformed disconnection complete event");
    return CommandChannel::EventCallbackResult::kContinue;
  }

  const auto& params = event.params<hci_spec::DisconnectionCompleteEventParams>();
  const auto event_handle = le16toh(params.connection_handle);

  // Silently ignore this event as it isn't meant for this connection.
  if (event_handle != handle) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  bt_log(INFO, "hci", "disconnection complete - %s, handle: %#.4x, reason: %#.2hhx (%s)",
         bt_str(event.ToResult()), handle, params.reason,
         hci_spec::StatusCodeToString(params.reason).c_str());

  if (self) {
    self->conn_state_ = State::kDisconnected;
  }

  // Peer disconnect. Callback may destroy connection.
  if (self && self->peer_disconnect_callback_) {
    self->peer_disconnect_callback_(self.get(), params.reason);
  }

  // Notify subclasses after peer_disconnect_callback_ has had a chance to clean up higher-level
  // connections.
  if (on_disconnection_complete) {
    on_disconnection_complete();
  }

  return CommandChannel::EventCallbackResult::kRemove;
}

void Connection::Disconnect(hci_spec::StatusCode reason) {
  BT_ASSERT(conn_state_ == Connection::State::kConnected);

  conn_state_ = Connection::State::kWaitingForDisconnectionComplete;

  // Here we send a HCI_Disconnect command without waiting for it to complete.
  auto status_cb = [](auto id, const EventPacket& event) {
    BT_DEBUG_ASSERT(event.event_code() == hci_spec::kCommandStatusEventCode);
    hci_is_error(event, TRACE, "hci", "ignoring disconnection failure");
  };

  auto disconn = EmbossCommandPacket::New<hci_spec::DisconnectCommandWriter>(hci_spec::kDisconnect);
  auto params = disconn.view_t();
  params.connection_handle().Write(handle());
  params.reason().Write(reason);

  bt_log(DEBUG, "hci", "disconnecting connection (handle: %#.4x, reason: %#.2hhx)", handle(),
         reason);

  // Send HCI Disconnect.
  hci_->command_channel()->SendCommand(std::move(disconn), std::move(status_cb),
                                       hci_spec::kCommandStatusEventCode);
}

}  // namespace bt::hci
