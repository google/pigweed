// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_

#include <lib/async/dispatcher.h>
#include <lib/sys/inspect/cpp/component.h>

#include "gap.h"
#include "low_energy_connection_handle.h"
#include "low_energy_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/generic_access_client.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::gap {

class LowEnergyConnectionManager;

namespace internal {

// Represents the state of an active connection. Each instance is owned
// and managed by a LowEnergyConnectionManager and is kept alive as long as
// there is at least one LowEnergyConnectionHandle that references it.
class LowEnergyConnection final : public sm::Delegate {
 public:
  LowEnergyConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link,
                      async_dispatcher_t* dispatcher,
                      fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
                      fbl::RefPtr<l2cap::L2cap> l2cap, fxl::WeakPtr<gatt::GATT> gatt,
                      fxl::WeakPtr<hci::Transport> transport, LowEnergyConnectionRequest request);

  // Notifies request callbacks and connection refs of the disconnection.
  ~LowEnergyConnection() override;

  // Add a request callback that will be called when NotifyRequestCallbacks is called or immediately
  // if it has already been called.
  void AddRequestCallback(LowEnergyConnectionRequest::ConnectionResultCallback cb);

  // Call request callbacks with references to this connection.
  void NotifyRequestCallbacks();

  // Create a reference to this connection. When the last reference is dropped, this connection will
  // be disconnected.
  std::unique_ptr<LowEnergyConnectionHandle> AddRef();

  // Decrements the ref count. Must be called when a LowEnergyConnectionHandle is
  // released/destroyed.
  void DropRef(LowEnergyConnectionHandle* ref);

  // Registers this connection with L2CAP and initializes the fixed channel
  // protocols.
  void InitializeFixedChannels(l2cap::LinkErrorCallback link_error_cb,
                               LowEnergyConnectionOptions connection_options);

  // Used to respond to protocol/service requests for increased security.
  void OnSecurityRequest(sm::SecurityLevel level, sm::StatusCallback cb);

  // Handles a pairing request (i.e. security upgrade) received from "higher levels", likely
  // initiated from GAP. This will only be used by pairing requests that are initiated
  // in the context of testing. May only be called on an already-established connection.
  void UpgradeSecurity(sm::SecurityLevel level, sm::BondableMode bondable_mode,
                       sm::StatusCallback cb);

  // Cancels any on-going pairing procedures and sets up SMP to use the provided
  // new I/O capabilities for future pairing procedures.
  void ResetSecurityManager(sm::IOCapability ioc);

  // Attach connection as child node of |parent| with specified |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  // Start kLEConnectionPauseCentral/Peripheral timeout that will update connection parameters.
  // Should be called as soon as this GAP connection is established.
  void StartConnectionPauseTimeout();

  void set_security_mode(LeSecurityMode mode) {
    ZX_ASSERT(sm_);
    sm_->set_security_mode(mode);
  }

  size_t ref_count() const { return refs_->size(); }

  PeerId peer_id() const { return peer_id_; }
  hci::ConnectionHandle handle() const { return link_->handle(); }
  hci::Connection* link() const { return link_.get(); }
  sm::BondableMode bondable_mode() const {
    ZX_ASSERT(sm_);
    return sm_->bondable_mode();
  }

  sm::SecurityProperties security() const {
    ZX_ASSERT(sm_);
    return sm_->security();
  }

  const std::optional<LowEnergyConnectionRequest>& request() { return request_; }

  // Take the request back from the connection for retrying the connection after a
  // kConnectionFailedToBeEstablished error.
  std::optional<LowEnergyConnectionRequest> take_request();

  fxl::WeakPtr<LowEnergyConnection> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // Start kLEConnectionPausePeripheral timeout that will send a connection parameter update
  // request. Should be called as soon as connection is established.
  void StartConnectionPausePeripheralTimeout();

  // Start kLEConnectionPauseCentral timeout that will update connection parameters.
  // Should be called as soon as connection is established.
  void StartConnectionPauseCentralTimeout();

  // Called by the L2CAP layer once the link has been registered and the fixed
  // channels have been opened.
  void OnL2capFixedChannelsOpened(fbl::RefPtr<l2cap::Channel> att, fbl::RefPtr<l2cap::Channel> smp,
                                  LowEnergyConnectionOptions connection_options);

  // Called when the preferred connection parameters have been received for a LE
  // peripheral. This can happen in the form of:
  //
  //   1. <<Slave Connection Interval Range>> advertising data field
  //   2. "Peripheral Preferred Connection Parameters" GATT characteristic
  //      (under "GAP" service)
  //   3. HCI LE Remote Connection Parameter Request Event
  //   4. L2CAP Connection Parameter Update request
  //
  // TODO(fxbug.dev/68803): Support #1 above.
  // TODO(fxbug.dev/68804): Support #3 above.
  //
  // This method caches |params| for later connection attempts and sends the
  // parameters to the controller if the initializing procedures are complete
  // (since we use more agressing initial parameters for pairing and service
  // discovery, as recommended by the specification in v5.0, Vol 3, Part C,
  // Section 9.3.12.1).
  //
  // |peer_id| uniquely identifies the peer. |handle| represents
  // the logical link that |params| should be applied to.
  void OnNewLEConnectionParams(const hci::LEPreferredConnectionParameters& params);

  // As an LE peripheral, request that the connection parameters |params| be used on the given
  // connection |conn| with peer |peer_id|. This may send an HCI LE Connection Update command or an
  // L2CAP Connection Parameter Update Request depending on what the local and remote controllers
  // support.
  //
  // Interrogation must have completed before this may be called.
  void RequestConnectionParameterUpdate(const hci::LEPreferredConnectionParameters& params);

  // Handler for connection parameter update command sent when an update is requested by
  // RequestConnectionParameterUpdate.
  //
  // If the HCI LE Connection Update command fails with status kUnsupportedRemoteFeature, the update
  // will be retried with an L2CAP Connection Parameter Update Request.
  void HandleRequestConnectionParameterUpdateCommandStatus(
      hci::LEPreferredConnectionParameters params, hci::Status status);

  // As an LE peripheral, send an L2CAP Connection Parameter Update Request requesting |params| on
  // the LE signaling channel of the given logical link |handle|.
  //
  // NOTE: This should only be used if the LE peripheral and/or LE central do not support the
  // Connection Parameters Request Link Layer Control Procedure (Core Spec v5.2  Vol 3, Part A,
  // Sec 4.20). If they do, UpdateConnectionParams(...) should be used instead.
  void L2capRequestConnectionParameterUpdate(const hci::LEPreferredConnectionParameters& params);

  // Requests that the controller use the given connection |params| by sending an HCI LE Connection
  // Update command. This may be issued on both the LE peripheral and the LE central.
  //
  // The link layer may modify the preferred parameters |params| before initiating the Connection
  // Parameters Request Link Layer Control Procedure (Core Spec v5.2, Vol 6, Part B, Sec 5.1.7).
  //
  // If non-null, |status_cb| will be called when the HCI Command Status event is received.
  //
  // The HCI LE Connection Update Complete event will be generated after the parameters have been
  // applied or if the update fails, and will indicate the (possibly modified) parameter values.
  //
  // NOTE: If the local host is an LE peripheral, then the local controller and the remote
  // LE central must have indicated support for this procedure in the LE feature mask. Otherwise,
  // L2capRequestConnectionParameterUpdate(...) should be used intead.
  using StatusCallback = fit::callback<void(hci::Status)>;
  void UpdateConnectionParams(const hci::LEPreferredConnectionParameters& params,
                              StatusCallback status_cb = nullptr);

  // This event may be generated without host interaction by the Link Layer, or as the result of a
  // Connection Update Command sent by either device, which is why it is not simply handled by the
  // command handler. (See Core Spec v5.2, Vol 6, Part B, Sec 5.1.7.1).
  void OnLEConnectionUpdateComplete(const hci::EventPacket& event);

  // Registers the peer with GATT and initiates service discovery. If |service_uuid| is specified,
  // only discover the indicated service and the GAP service.
  void InitializeGatt(fbl::RefPtr<l2cap::Channel> att, std::optional<UUID> service_uuid);

  // Called when service discovery completes. |services| will only include services with the GAP
  // UUID (there should only be one, but this is not guaranteed).
  void OnGattServicesResult(att::Status status, gatt::ServiceList services);

  // Notifies all connection refs of disconnection.
  void CloseRefs();

  // sm::Delegate overrides:
  void OnNewPairingData(const sm::PairingData& pairing_data) override;
  void OnPairingComplete(sm::Status status) override;
  void OnAuthenticationFailure(hci::Status status) override;
  void OnNewSecurityProperties(const sm::SecurityProperties& sec) override;
  std::optional<sm::IdentityInfo> OnIdentityInformationRequest() override;
  void ConfirmPairing(ConfirmCallback confirm) override;
  void DisplayPasskey(uint32_t passkey, sm::Delegate::DisplayMethod method,
                      ConfirmCallback confirm) override;
  void RequestPasskey(PasskeyResponseCallback respond) override;

  PeerId peer_id_;
  fxl::WeakPtr<Peer> peer_;
  std::unique_ptr<hci::Connection> link_;
  async_dispatcher_t* dispatcher_;
  fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr_;

  struct InspectProperties {
    inspect::StringProperty peer_id;
    inspect::StringProperty peer_address;
  };
  InspectProperties inspect_properties_;
  inspect::Node inspect_node_;

  // Reference to the data plane is used to update the L2CAP layer to
  // reflect the correct link security level.
  fbl::RefPtr<l2cap::L2cap> l2cap_;

  // Reference to the GATT profile layer is used to initiate service discovery
  // and register the link.
  fxl::WeakPtr<gatt::GATT> gatt_;

  // SMP pairing manager.
  std::unique_ptr<sm::SecurityManager> sm_;

  fxl::WeakPtr<hci::Transport> transport_;

  // Event handler ID for the HCI LE Connection Update Complete event.
  hci::CommandChannel::EventHandlerId conn_update_cmpl_handler_id_;

  // Called with the status of the next HCI LE Connection Update Complete event.
  // The HCI LE Connection Update command does not have its own complete event handler because the
  // HCI LE Connection Complete event can be generated for other reasons.
  fit::callback<void(hci::StatusCode)> le_conn_update_complete_command_callback_;

  // Called after kLEConnectionPausePeripheral.
  std::optional<async::TaskClosure> conn_pause_peripheral_timeout_;

  // Called after kLEConnectionPauseCentral.
  std::optional<async::TaskClosure> conn_pause_central_timeout_;

  // Request callbacks that will be notified by |NotifyRequestCallbacks()| when interrogation
  // completes or by the dtor.
  std::optional<LowEnergyConnectionRequest> request_;

  // LowEnergyConnectionManager is responsible for making sure that these
  // pointers are always valid.
  using ConnectionHandleSet = std::unordered_set<LowEnergyConnectionHandle*>;
  IntInspectable<ConnectionHandleSet> refs_;

  // Null until service discovery completes.
  std::optional<GenericAccessClient> gap_service_client_;

  fxl::WeakPtrFactory<LowEnergyConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnection);
};

}  // namespace internal
}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_
