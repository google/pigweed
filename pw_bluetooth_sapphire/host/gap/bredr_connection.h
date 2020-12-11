// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sco/sco_connection_manager.h"

namespace bt::gap {

class PeerCache;
class PairingState;

// Represents an ACL connection that is currently open with the controller (i.e. after receiving a
// Connection Complete and before either user disconnection or Disconnection Complete).
class BrEdrConnection final {
 public:
  // |send_auth_request_cb| is called during pairing, and should send the authenticaion request HCI
  // command.
  // |disconnect_cb| is called when an error occurs and the link should be disconnected.
  // |on_peer_disconnect_cb| is called when the peer disconnects and this connection should be
  // destroyed.
  using Request = ConnectionRequest<BrEdrConnection*>;
  BrEdrConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link,
                  fit::closure send_auth_request_cb, fit::closure disconnect_cb,
                  fit::closure on_peer_disconnect_cb, PeerCache* peer_cache,
                  fbl::RefPtr<l2cap::L2cap> l2cap, fxl::WeakPtr<hci::Transport> transport,
                  std::optional<Request> request);

  ~BrEdrConnection();

  BrEdrConnection(BrEdrConnection&&) = default;
  BrEdrConnection& operator=(BrEdrConnection&&) = default;

  // Called after interrogation completes to mark this connection as available for upper layers,
  // i.e. L2CAP. Also signals any requesters with a successful status and this
  // connection. If not called and this connection is deleted (e.g. by disconnection), requesters
  // will be signaled with |HostError::kNotSupported| (to indicate interrogation error).
  void Start();

  // Add a request callback that will be called when Start() is called (or immediately if Start()
  // has already been called).
  void AddRequestCallback(Request::OnComplete cb);

  // If |Start| has been called, opens an L2CAP channel using the preferred parameters |params| on
  // the L2cap provided. Otherwise, calls |cb| with a nullptr.
  void OpenL2capChannel(l2cap::PSM psm, l2cap::ChannelParameters params, l2cap::ChannelCallback cb);

  // Open a SCO connection associated with this BR/EDR connection.
  // |initiator| indicates whether a connection request should be sent or a connection
  // request is expected.
  // |parameters| is the set of SCO connection parameters that the connection should be configured
  // with.
  // |callback| will be called with the result of the procedure.
  //
  // Returns a handle that will cancel the pending request if destroyed.
  using ScoConnectionCallback = sco::ScoConnectionManager::ConnectionCallback;
  using ScoRequestHandle = sco::ScoConnectionManager::RequestHandle;
  ScoRequestHandle OpenScoConnection(bool initiator,
                                     hci::SynchronousConnectionParameters parameters,
                                     ScoConnectionCallback callback);

  const hci::Connection& link() const { return *link_; }
  hci::Connection& link() { return *link_; }
  PeerId peer_id() const { return peer_id_; }
  PairingState& pairing_state() { return *pairing_state_; }

 private:
  // True if Start() has been called.
  bool ready_;
  PeerId peer_id_;
  std::unique_ptr<hci::Connection> link_;
  std::unique_ptr<PairingState> pairing_state_;
  std::optional<Request> request_;
  fbl::RefPtr<l2cap::L2cap> domain_;
  std::unique_ptr<sco::ScoConnectionManager> sco_manager_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnection);
};

}  // namespace bt::gap

#endif  //  SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_
