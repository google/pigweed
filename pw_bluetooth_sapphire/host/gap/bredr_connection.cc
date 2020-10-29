// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection.h"

namespace bt::gap {

BrEdrConnection::BrEdrConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link,
                                 fit::closure send_auth_request_cb, fit::closure disconnect_cb,
                                 fit::closure on_peer_disconnect_cb, PeerCache* peer_cache,
                                 fbl::RefPtr<l2cap::L2cap> l2cap,
                                 fxl::WeakPtr<hci::Transport> transport,
                                 std::optional<Request> request)
    : ready_(false),
      peer_id_(peer_id),
      link_(std::move(link)),
      pairing_state_(std::make_unique<PairingState>(
          peer_id, link_.get(), peer_cache, std::move(send_auth_request_cb),
          [peer_id, disconnect_cb = std::move(disconnect_cb)](auto, hci::Status status) {
            if (bt_is_error(status, DEBUG, "gap-bredr",
                            "PairingState error status, disconnecting (peer id: %s)",
                            bt_str(peer_id))) {
              disconnect_cb();
            }
          })),
      request_(std::move(request)),
      domain_(std::move(l2cap)),
      sco_manager_(std::make_unique<sco::ScoConnectionManager>(
          peer_id_, link_->handle(), link_->peer_address(), link_->local_address(), transport)) {
  link_->set_peer_disconnect_callback(
      [peer_disconnect_cb = std::move(on_peer_disconnect_cb)](auto conn) { peer_disconnect_cb(); });
}

BrEdrConnection::~BrEdrConnection() {
  if (request_.has_value()) {
    // Connection never completed so signal the requester(s).
    request_->NotifyCallbacks(hci::Status(HostError::kNotSupported), [] { return nullptr; });
  }

  sco_manager_.reset();
  pairing_state_.reset();
  link_.reset();
}

void BrEdrConnection::Start() {
  ZX_ASSERT_MSG(!ready_, "Start on a connection that's already started");
  ready_ = true;

  // Fulfill and clear request so that the dtor does not signal requester(s) with errors.
  if (auto request = std::exchange(request_, std::nullopt); request.has_value()) {
    request->NotifyCallbacks(hci::Status(), [this] { return this; });
  }
}

void BrEdrConnection::OpenL2capChannel(l2cap::PSM psm, l2cap::ChannelParameters params,
                                       l2cap::ChannelCallback cb) {
  if (!ready_) {
    // Connection is not yet ready for L2CAP; return a null channel.
    bt_log(INFO, "gap-bredr", "Connection to %s not complete; canceling channel to PSM %.4x",
           bt_str(peer_id()), psm);
    cb(nullptr);
    return;
  }

  bt_log(TRACE, "gap-bredr", "opening l2cap channel on %#.4x for %s", psm, bt_str(peer_id()));
  domain_->OpenL2capChannel(link().handle(), psm, params, std::move(cb));
}

BrEdrConnection::ScoRequestHandle BrEdrConnection::OpenScoConnection(
    bool initiator, hci::SynchronousConnectionParameters parameters,
    ScoConnectionCallback callback) {
  if (initiator) {
    return sco_manager_->OpenConnection(parameters, std::move(callback));
  }
  return sco_manager_->AcceptConnection(parameters, std::move(callback));
}

}  // namespace bt::gap
