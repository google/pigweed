// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_request.h"

#include "lib/async/cpp/time.h"
#include "lib/async/default.h"

namespace bt::gap {

namespace {

const char* const kInspectHasIncomingPropertyName = "has_incoming";
const char* const kInspectCallbacksPropertyName = "callbacks";
const char* const kInspectFirstCreateConnectionReqMadeName =
    "first_create_connection_request_timestamp";
const char* const kInspectPeerIdPropertyName = "peer_id";
constexpr zx::duration kRetryWindowAfterFirstCreateConn = zx::sec(30);

}  // namespace

BrEdrConnectionRequest::BrEdrConnectionRequest(const DeviceAddress& addr, PeerId peer_id,
                                               Peer::InitializingConnectionToken token)
    : peer_id_(peer_id),
      address_(addr),
      callbacks_(/*convert=*/[](auto& c) { return c.size(); }),
      has_incoming_(false),
      first_create_connection_req_made_(
          std::nullopt, [](const std::optional<zx::time>& t) { return t ? t->get() : -1; }),
      peer_init_conn_token_(std::move(token)) {}

BrEdrConnectionRequest::BrEdrConnectionRequest(const DeviceAddress& addr, PeerId peer_id,
                                               Peer::InitializingConnectionToken token,
                                               OnComplete&& callback)
    : BrEdrConnectionRequest(addr, peer_id, std::move(token)) {
  callbacks_.Mutable()->push_back(std::move(callback));
}

void BrEdrConnectionRequest::NotifyCallbacks(hci::Result<> status, const RefFactory& generate_ref) {
  // Clear token before notifying callbacks so that connection state change is reflected in
  // callbacks.
  peer_init_conn_token_.reset();

  // If this request has been moved from, |callbacks_| may be empty.
  for (const auto& callback : *callbacks_) {
    callback(status, generate_ref());
  }
}

void BrEdrConnectionRequest::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  has_incoming_.AttachInspect(inspect_node_, kInspectHasIncomingPropertyName);
  callbacks_.AttachInspect(inspect_node_, kInspectCallbacksPropertyName);
  first_create_connection_req_made_.AttachInspect(inspect_node_,
                                                  kInspectFirstCreateConnectionReqMadeName);
  peer_id_property_ = inspect_node_.CreateString(kInspectPeerIdPropertyName, peer_id_.ToString());
}

void BrEdrConnectionRequest::RecordHciCreateConnectionAttempt() {
  if (!first_create_connection_req_made_.value()) {
    first_create_connection_req_made_.Set(async::Now(async_get_default_dispatcher()));
  }
}

bool BrEdrConnectionRequest::ShouldRetry(hci::Error failure_mode) {
  zx::time now = async::Now(async_get_default_dispatcher());
  std::optional<zx::time> first_create_conn_req_made = first_create_connection_req_made_.value();
  return failure_mode.is(hci_spec::StatusCode::PAGE_TIMEOUT) &&
         first_create_conn_req_made.has_value() &&
         now - *first_create_conn_req_made < kRetryWindowAfterFirstCreateConn;
}
}  // namespace bt::gap
