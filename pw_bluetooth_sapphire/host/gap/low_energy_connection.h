// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_

#include <lib/async/dispatcher.h>

#include "gap.h"
#include "low_energy_connection_handle.h"
#include "low_energy_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
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
                      LowEnergyConnectionRequest request);

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
  void InitializeFixedChannels(l2cap::LEConnectionParameterUpdateCallback cp_cb,
                               l2cap::LinkErrorCallback link_error_cb,
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

  // Set callback that will be called after the kLEConnectionPausePeripheral timeout, or now if the
  // timeout has already finished.
  void on_peripheral_pause_timeout(fit::callback<void(LowEnergyConnection*)> callback);

  // Should be called as soon as connection is established.
  // Calls |conn_pause_peripheral_callback_| after kLEConnectionPausePeripheral.
  void StartConnectionPausePeripheralTimeout();

  // Posts |callback| to be called kLEConnectionPauseCentral after this connection was established.
  void PostCentralPauseTimeoutCallback(fit::callback<void()> callback);

  void set_security_mode(LeSecurityMode mode) {
    ZX_ASSERT(sm_);
    sm_->set_security_mode(mode);
  }

  size_t ref_count() const { return refs_.size(); }

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
  // Called by the L2CAP layer once the link has been registered and the fixed
  // channels have been opened.
  void OnL2capFixedChannelsOpened(fbl::RefPtr<l2cap::Channel> att, fbl::RefPtr<l2cap::Channel> smp,
                                  LowEnergyConnectionOptions connection_options);

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
  std::unique_ptr<hci::Connection> link_;
  async_dispatcher_t* dispatcher_;
  fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr_;

  // Reference to the data plane is used to update the L2CAP layer to
  // reflect the correct link security level.
  fbl::RefPtr<l2cap::L2cap> l2cap_;

  // Reference to the GATT profile layer is used to initiate service discovery
  // and register the link.
  fxl::WeakPtr<gatt::GATT> gatt_;

  // SMP pairing manager.
  std::unique_ptr<sm::SecurityManager> sm_;

  // Called after kLEConnectionPausePeripheral.
  std::optional<async::TaskClosure> conn_pause_peripheral_timeout_;

  // Called by |conn_pause_peripheral_timeout_|.
  fit::callback<void(LowEnergyConnection*)> conn_pause_peripheral_callback_;

  // Set to the time when connection parameters should be sent as LE central.
  const zx::time conn_pause_central_expiry_;

  // Request callbacks that will be notified by |NotifyRequestCallbacks()| when interrogation
  // completes or by the dtor.
  std::optional<LowEnergyConnectionRequest> request_;

  // LowEnergyConnectionManager is responsible for making sure that these
  // pointers are always valid.
  std::unordered_set<LowEnergyConnectionHandle*> refs_;

  // Null until service discovery completes.
  std::optional<GenericAccessClient> gap_service_client_;

  fxl::WeakPtrFactory<LowEnergyConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnection);
};

}  // namespace internal
}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_H_
