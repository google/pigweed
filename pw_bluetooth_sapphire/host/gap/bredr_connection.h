// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_

#include <lib/async/cpp/time.h>
#include <lib/async/default.h>

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
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
  using Request = BrEdrConnectionRequest;
  BrEdrConnection(fxl::WeakPtr<Peer> peer, std::unique_ptr<hci::BrEdrConnection> link,
                  fit::closure send_auth_request_cb, fit::callback<void()> disconnect_cb,
                  fit::closure on_peer_disconnect_cb, l2cap::ChannelManager* l2cap,
                  fxl::WeakPtr<hci::Transport> transport, std::optional<Request> request);

  ~BrEdrConnection();

  BrEdrConnection(BrEdrConnection&&) = default;
  BrEdrConnection& operator=(BrEdrConnection&&) = default;

  void Interrogate(BrEdrInterrogator::ResultCallback callback);

  // Called after interrogation completes to mark this connection as available for upper layers,
  // i.e. L2CAP. Also signals any requesters with a successful status and this
  // connection. If not called and this connection is deleted (e.g. by disconnection), requesters
  // will be signaled with |HostError::kNotSupported| (to indicate interrogation error).
  void OnInterrogationComplete();

  // Add a request callback that will be called when OnInterrogationComplete() is called (or
  // immediately if OnInterrogationComplete() has already been called).
  void AddRequestCallback(Request::OnComplete cb);

  // If |OnInterrogationComplete| has been called, opens an L2CAP channel using the preferred
  // parameters |params| on the L2cap provided. Otherwise, calls |cb| with a nullptr.
  void OpenL2capChannel(l2cap::PSM psm, l2cap::ChannelParameters params, l2cap::ChannelCallback cb);

  // See ScoConnectionManager for documentation.
  using ScoRequestHandle = sco::ScoConnectionManager::RequestHandle;
  ScoRequestHandle OpenScoConnection(hci_spec::SynchronousConnectionParameters,
                                     sco::ScoConnectionManager::OpenConnectionCallback callback);
  ScoRequestHandle AcceptScoConnection(
      std::vector<hci_spec::SynchronousConnectionParameters> parameters,
      sco::ScoConnectionManager::AcceptConnectionCallback callback);

  // Attach connection inspect node as a child of |parent| named |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  const hci::Connection& link() const { return *link_; }
  hci::BrEdrConnection& link() { return *link_; }
  PeerId peer_id() const { return peer_id_; }
  PairingState& pairing_state() { return *pairing_state_; }

  // Returns the duration that this connection has been alive.
  zx::duration duration() const {
    return async::Now(async_get_default_dispatcher()) - create_time_;
  }

 private:
  // |conn_token| is a token received from Peer::MutBrEdr::RegisterConnection().
  void set_peer_connection_token(Peer::ConnectionToken conn_token);

  // Called by |pairing_state_| when pairing completes with |status|.
  void OnPairingStateStatus(hci_spec::ConnectionHandle handle, hci::Result<> status);

  bool interrogation_complete() const { return !request_.has_value(); }

  PeerId peer_id_;
  fxl::WeakPtr<Peer> peer_;
  std::unique_ptr<hci::BrEdrConnection> link_;
  std::optional<Request> request_;
  std::unique_ptr<PairingState> pairing_state_;
  l2cap::ChannelManager* l2cap_;
  std::unique_ptr<sco::ScoConnectionManager> sco_manager_;
  std::unique_ptr<BrEdrInterrogator> interrogator_;
  // Time this object was constructed.
  zx::time create_time_;
  // Called when an error occurs and this connection should be disconnected.
  fit::callback<void()> disconnect_cb_;

  struct InspectProperties {
    inspect::StringProperty peer_id;
  };
  InspectProperties inspect_properties_;
  inspect::Node inspect_node_;

  std::optional<Peer::InitializingConnectionToken> peer_init_token_;
  // Ensures that this peer is marked "connected" once pairing completes.
  // Unregisters the connection from PeerCache when this connection is destroyed.
  Peer::ConnectionToken peer_conn_token_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnection);
};

}  // namespace bt::gap

#endif  //  SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_H_
