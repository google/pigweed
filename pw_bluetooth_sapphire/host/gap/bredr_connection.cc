// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection.h"

#include <lib/async/default.h>

#include <utility>

namespace bt::gap {

namespace {

const char* const kInspectPeerIdPropertyName = "peer_id";

}

BrEdrConnection::BrEdrConnection(fxl::WeakPtr<Peer> peer,
                                 std::unique_ptr<hci::BrEdrConnection> link,
                                 fit::closure send_auth_request_cb,
                                 fit::callback<void()> disconnect_cb,
                                 fit::closure on_peer_disconnect_cb, l2cap::ChannelManager* l2cap,
                                 hci::Transport::WeakPtr transport, std::optional<Request> request)
    : peer_id_(peer->identifier()),
      peer_(std::move(peer)),
      link_(std::move(link)),
      request_(std::move(request)),
      pairing_state_(std::make_unique<PairingState>(
          peer_, link_.get(), request_ && request_->AwaitingOutgoing(),
          std::move(send_auth_request_cb),
          fit::bind_member<&BrEdrConnection::OnPairingStateStatus>(this))),
      l2cap_(l2cap),
      sco_manager_(std::make_unique<sco::ScoConnectionManager>(
          peer_id_, link_->handle(), link_->peer_address(), link_->local_address(), transport)),
      interrogator_(
          new BrEdrInterrogator(peer_, link_->handle(), transport->command_channel()->AsWeakPtr())),
      create_time_(async::Now(async_get_default_dispatcher())),
      disconnect_cb_(std::move(disconnect_cb)),
      peer_init_token_(request_->take_peer_init_token()),
      peer_conn_token_(peer_->MutBrEdr().RegisterConnection()) {
  link_->set_peer_disconnect_callback([peer_disconnect_cb = std::move(on_peer_disconnect_cb)](
                                          auto conn, auto /*reason*/) { peer_disconnect_cb(); });
}

BrEdrConnection::~BrEdrConnection() {
  if (auto request = std::exchange(request_, std::nullopt); request.has_value()) {
    // Connection never completed so signal the requester(s).
    request->NotifyCallbacks(ToResult(HostError::kNotSupported), [] { return nullptr; });
  }

  sco_manager_.reset();
  pairing_state_.reset();
  link_.reset();
}

void BrEdrConnection::Interrogate(BrEdrInterrogator::ResultCallback callback) {
  interrogator_->Start(std::move(callback));
}

void BrEdrConnection::OnInterrogationComplete() {
  BT_ASSERT_MSG(!interrogation_complete(), "%s on a connection that's already been interrogated",
                __FUNCTION__);

  // Fulfill and clear request so that the dtor does not signal requester(s) with errors.
  if (auto request = std::exchange(request_, std::nullopt); request.has_value()) {
    request->NotifyCallbacks(fit::ok(), [this] { return this; });
  }
}

void BrEdrConnection::AddRequestCallback(BrEdrConnection::Request::OnComplete cb) {
  if (!request_.has_value()) {
    cb(fit::ok(), this);
    return;
  }

  BT_ASSERT(request_);
  request_->AddCallback(std::move(cb));
}

void BrEdrConnection::OpenL2capChannel(l2cap::PSM psm, l2cap::ChannelParameters params,
                                       l2cap::ChannelCallback cb) {
  if (!interrogation_complete()) {
    // Connection is not yet ready for L2CAP; return a null channel.
    bt_log(INFO, "gap-bredr", "connection not ready; canceling connect to PSM %.4x (peer: %s)", psm,
           bt_str(peer_id_));
    cb(nullptr);
    return;
  }

  bt_log(INFO, "gap-bredr", "opening l2cap channel on psm %#.4x (peer: %s)", psm, bt_str(peer_id_));
  l2cap_->OpenL2capChannel(link().handle(), psm, params, std::move(cb));
}

BrEdrConnection::ScoRequestHandle BrEdrConnection::OpenScoConnection(
    bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter> parameters,
    sco::ScoConnectionManager::OpenConnectionCallback callback) {
  return sco_manager_->OpenConnection(std::move(parameters), std::move(callback));
}

BrEdrConnection::ScoRequestHandle BrEdrConnection::AcceptScoConnection(
    std::vector<bt::StaticPacket<hci_spec::SynchronousConnectionParametersWriter>> parameters,
    sco::ScoConnectionManager::AcceptConnectionCallback callback) {
  return sco_manager_->AcceptConnection(std::move(parameters), std::move(callback));
}

void BrEdrConnection::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  inspect_properties_.peer_id =
      inspect_node_.CreateString(kInspectPeerIdPropertyName, peer_id_.ToString());
}

void BrEdrConnection::OnPairingStateStatus(hci_spec::ConnectionHandle handle,
                                           hci::Result<> status) {
  if (bt_is_error(status, DEBUG, "gap-bredr",
                  "PairingState error status, disconnecting (peer id: %s)", bt_str(peer_id_))) {
    if (disconnect_cb_) {
      disconnect_cb_();
    }
    return;
  }

  // Once pairing succeeds for the first time, the transition from Initializing -> Connected can
  // happen.
  peer_init_token_.reset();
}

}  // namespace bt::gap
