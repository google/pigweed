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
  // Request handle returned to clients. Cancels request when destroyed.
  class RequestHandle final {
   public:
    explicit RequestHandle(fit::callback<void()> on_cancel) : on_cancel_(std::move(on_cancel)) {}
    RequestHandle(RequestHandle&&) = default;
    RequestHandle& operator=(RequestHandle&&) = default;
    ~RequestHandle() { Cancel(); }

    void Cancel() {
      if (on_cancel_) {
        on_cancel_();
      }
    }

   private:
    fit::callback<void()> on_cancel_;
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(RequestHandle);
  };

  // |peer_id| corresponds to the peer associated with this BR/EDR connection.
  // |acl_handle| corresponds to the ACL connection associated with these SCO connections.
  // |transport| must outlive this object.
  ScoConnectionManager(PeerId peer_id, hci::ConnectionHandle acl_handle, DeviceAddress peer_address,
                       DeviceAddress local_address, fxl::WeakPtr<hci::Transport> transport);
  // Closes connections and cancels connection requests.
  ~ScoConnectionManager();

  // Initiate and outbound connection. A request will be queued if a connection is already in
  // progress. On error, |callback| will be called with nullptr.
  // Returns a handle that will cancel the request when dropped (if connection establishment has not
  // started).
  using ConnectionCallback = fit::callback<void(fbl::RefPtr<ScoConnection>)>;
  RequestHandle OpenConnection(hci::SynchronousConnectionParameters parameters,
                               ConnectionCallback callback);

  // Accept the next inbound connection request and establish a new SCO connection using
  // |parameters|.
  // On error, |callback| will be called with nullptr.
  // If another Open/Accept request is made before the peer sends a connection request, this request
  // will be cancelled.
  // Returns a handle that will cancel the request when dropped (if connection establishment has not
  // started).
  RequestHandle AcceptConnection(hci::SynchronousConnectionParameters parameters,
                                 ConnectionCallback callback);

 private:
  using ScoRequestId = uint64_t;

  class ConnectionRequest final {
   public:
    ConnectionRequest(ConnectionRequest&&) = default;
    ConnectionRequest& operator=(ConnectionRequest&&) = default;
    ~ConnectionRequest() {
      if (callback) {
        bt_log(DEBUG, "sco", "Cancelling SCO connection request (id: %zu)", id);
        callback(nullptr);
      }
    }

    ScoRequestId id;
    bool initiator;
    bool received_request;
    hci::SynchronousConnectionParameters parameters;
    ConnectionCallback callback;
  };

  hci::CommandChannel::EventHandlerId AddEventHandler(const hci::EventCode& code,
                                                      hci::CommandChannel::EventCallback cb);

  // Event handlers:
  hci::CommandChannel::EventCallbackResult OnSynchronousConnectionComplete(
      const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnConnectionRequest(const hci::EventPacket& event);

  ScoConnectionManager::RequestHandle QueueRequest(bool initiator,
                                                   hci::SynchronousConnectionParameters,
                                                   ConnectionCallback);

  void TryCreateNextConnection();

  void CompleteRequest(fbl::RefPtr<ScoConnection> connection);

  void SendCommandWithStatusCallback(std::unique_ptr<hci::CommandPacket> command_packet,
                                     hci::StatusCallback cb);

  // If either the queued or in progress request has the given id and can be cancelled, cancel it.
  // Called when a RequestHandle is dropped.
  void CancelRequestWithId(ScoRequestId);

  // The id that should be associated with the next request. Incremented when the current value is
  // used.
  ScoRequestId next_req_id_;

  // If a request is made while in_progress_request_ is waiting for a complete event, it gets queued
  // in queued_request_.
  std::optional<ConnectionRequest> queued_request_;

  std::optional<ConnectionRequest> in_progress_request_;

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
