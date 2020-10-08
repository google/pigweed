// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_MANAGER_H_

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/sco_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::gap {

// ScoConnectionManager handles SCO connections for a single BR/EDR connection. This includes
// queuing outbound and inbound connection requests and handling events related to SCO connections.
class ScoConnectionManager final {
 public:
  // |peer_id| corresponds to the peer associated with this BR/EDR connection.
  // |acl_handle| corresponds to the ACL connection associated with these SCO connections.
  // |transport| must outlive this object.
  ScoConnectionManager(PeerId peer_id, hci::ConnectionHandle acl_handle, DeviceAddress peer_address,
                       DeviceAddress local_address, fxl::WeakPtr<hci::Transport> transport);
  ~ScoConnectionManager();

  // On error, |callback| will be called with nullptr.
  using ConnectionCallback = fit::function<void(fbl::RefPtr<ScoConnection>)>;
  void OpenConnection(hci::SynchronousConnectionParameters parameters, ConnectionCallback callback);

  // TODO(fxbug.dev/58458): Add method for accepting a connection request

 private:
  hci::CommandChannel::EventHandlerId AddEventHandler(const hci::EventCode& code,
                                                      hci::CommandChannel::EventCallback cb);

  // Event handlers:
  hci::CommandChannel::EventCallbackResult OnSynchronousConnectionComplete(
      const hci::EventPacket& event);

  void TryCreateNextConnection();

  void CompleteRequest(fbl::RefPtr<ScoConnection> connection);

  void SendCommandWithStatusCallback(std::unique_ptr<hci::CommandPacket> command_packet,
                                     hci::StatusCallback cb);

  struct ConnectionRequest {
    bool initiator;
    hci::SynchronousConnectionParameters parameters;
    ConnectionCallback callback;
  };

  std::queue<ConnectionRequest> connection_requests_;

  std::optional<ConnectionRequest> in_progress_request_;

  // TODO(fxbug.dev/58458): Add timeout for responder requests.

  // Holds active connections.
  std::unordered_map<hci::ConnectionHandle, fbl::RefPtr<ScoConnection>> connections_;

  // Handler IDs for registered events
  std::vector<hci::CommandChannel::EventHandlerId> event_handler_ids_;

  PeerId peer_id_;

  const DeviceAddress local_address_;
  const DeviceAddress peer_address_;

  hci::ConnectionHandle acl_handle_;

  fxl::WeakPtr<hci::Transport> transport_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<ScoConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScoConnectionManager);
};
}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_SCO_CONNECTION_MANAGER_H_
