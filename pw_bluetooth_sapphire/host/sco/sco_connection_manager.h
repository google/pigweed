// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_MANAGER_H_

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/sco/sco_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sco {

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
  ScoConnectionManager(PeerId peer_id, hci_spec::ConnectionHandle acl_handle,
                       DeviceAddress peer_address, DeviceAddress local_address,
                       fxl::WeakPtr<hci::Transport> transport);
  // Closes connections and cancels connection requests.
  ~ScoConnectionManager();

  // Initiate and outbound connection. A request will be queued if a connection is already in
  // progress. On error, |callback| will be called with an error result. The error will be
  // |kCanceled| if a connection was never attempted, or |kFailed| if establishing a connection
  // failed. Returns a handle that will cancel the request when dropped (if connection establishment
  // has not started).
  using OpenConnectionResult = fitx::result<HostError, fxl::WeakPtr<ScoConnection>>;
  using OpenConnectionCallback = fit::callback<void(OpenConnectionResult)>;
  RequestHandle OpenConnection(hci_spec::SynchronousConnectionParameters parameters,
                               OpenConnectionCallback callback);

  // Accept inbound connection requests using the parameters given in order. The parameters will be
  // tried in order until either a connection is successful, all parameters have been rejected, or
  // the procedure is canceled. On success, |callback| will be called with the connection object and
  // the index of the parameters used to establish the connection. On error, |callback| will be
  // called with an error result. If another Open/Accept request is made before a connection request
  // is received, this request will be canceled (with error |kCanceled|). Returns a handle that will
  // cancel the request when destroyed (if connection establishment has not started).
  using AcceptConnectionResult =
      fitx::result<HostError,
                   std::pair<fxl::WeakPtr<ScoConnection>, size_t /*index of parameters used*/>>;
  using AcceptConnectionCallback = fit::callback<void(AcceptConnectionResult)>;
  RequestHandle AcceptConnection(std::vector<hci_spec::SynchronousConnectionParameters> parameters,
                                 AcceptConnectionCallback callback);

 private:
  using ScoRequestId = uint64_t;
  using ConnectionResult =
      fitx::result<HostError,
                   std::pair<fxl::WeakPtr<ScoConnection>, size_t /*index of parameters used*/>>;
  using ConnectionCallback = fit::callback<void(ConnectionResult)>;

  class ConnectionRequest final {
   public:
    ConnectionRequest(ScoRequestId id_arg, bool initiator_arg, bool received_request_arg,
                      std::vector<hci_spec::SynchronousConnectionParameters> parameters_arg,
                      ConnectionCallback callback_arg)
        : id(id_arg),
          initiator(initiator_arg),
          received_request(received_request_arg),
          parameters(std::move(parameters_arg)),
          callback(std::move(callback_arg)) {}
    ConnectionRequest(ConnectionRequest&&) = default;
    ConnectionRequest& operator=(ConnectionRequest&&) = default;
    ~ConnectionRequest() {
      if (callback) {
        bt_log(DEBUG, "sco", "Cancelling SCO connection request (id: %zu)", id);
        callback(fitx::error(HostError::kCanceled));
      }
    }

    ScoRequestId id;
    bool initiator;
    bool received_request;
    size_t current_param_index = 0;
    std::vector<hci_spec::SynchronousConnectionParameters> parameters;
    ConnectionCallback callback;
  };

  hci::CommandChannel::EventHandlerId AddEventHandler(const hci_spec::EventCode& code,
                                                      hci::CommandChannel::EventCallback cb);

  // Event handlers:
  hci::CommandChannel::EventCallbackResult OnSynchronousConnectionComplete(
      const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnConnectionRequest(const hci::EventPacket& event);

  // Returns true if parameters matching the corresponding transport were found in the current
  // request, or false otherwise. Mutates the current request's parameter index to that of the
  // matching parameters (or past the end on failure).
  bool FindNextParametersThatSupportSco();
  bool FindNextParametersThatSupportEsco();

  ScoConnectionManager::RequestHandle QueueRequest(
      bool initiator, std::vector<hci_spec::SynchronousConnectionParameters>, ConnectionCallback);

  void TryCreateNextConnection();

  void CompleteRequestOrTryNextParameters(ConnectionResult);

  void CompleteRequest(ConnectionResult);

  void SendCommandWithStatusCallback(std::unique_ptr<hci::CommandPacket> command_packet,
                                     hci::ResultFunction<> cb);

  void SendRejectConnectionCommand(DeviceAddressBytes addr, hci_spec::StatusCode reason);

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
  std::unordered_map<hci_spec::ConnectionHandle, std::unique_ptr<ScoConnection>> connections_;

  // Handler IDs for registered events
  std::vector<hci::CommandChannel::EventHandlerId> event_handler_ids_;

  PeerId peer_id_;

  const DeviceAddress local_address_;
  const DeviceAddress peer_address_;

  hci_spec::ConnectionHandle acl_handle_;

  fxl::WeakPtr<hci::Transport> transport_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<ScoConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScoConnectionManager);
};
}  // namespace bt::sco

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SCO_SCO_CONNECTION_MANAGER_H_
