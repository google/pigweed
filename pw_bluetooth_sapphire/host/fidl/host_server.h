// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_

#include <fuchsia/bluetooth/host/cpp/fidl.h>
#include <lib/zx/channel.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/lib/fidl/hanging_getter.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Custom hanging getter for the `WatchPeers()` method. Here we keep track of each `updated` and
// `removed` notification per PeerId so that the hanging get contains no duplicates and removed
// entries aren't reflected in `updated`.
class PeerTracker {
 public:
  using Updated = std::vector<fuchsia::bluetooth::sys::Peer>;
  using Removed = std::vector<fuchsia::bluetooth::PeerId>;

  PeerTracker() = default;
  PeerTracker(PeerTracker&&) = default;
  PeerTracker& operator=(PeerTracker&&) = default;

  // Returns parameters that can be used in a WatchPeers() response.
  std::pair<Updated, Removed> ToFidl(const bt::gap::PeerCache* peer_cache);

  void Update(bt::PeerId id);
  void Remove(bt::PeerId id);

 private:
  std::unordered_set<bt::PeerId> updated_;
  std::unordered_set<bt::PeerId> removed_;
};

class WatchPeersGetter
    : public bt_lib_fidl::HangingGetterBase<PeerTracker,
                                            void(PeerTracker::Updated, PeerTracker::Removed)> {
 public:
  explicit WatchPeersGetter(bt::gap::PeerCache* peer_cache);

 protected:
  void Notify(std::queue<Callback> callbacks, PeerTracker peers) override;

 private:
  bt::gap::PeerCache* peer_cache_;  // weak
};

// Implements the Host FIDL interface. Owns all FIDL connections that have been
// opened through it.
class HostServer : public AdapterServerBase<fuchsia::bluetooth::host::Host>,
                   public bt::gap::PairingDelegate {
 public:
  HostServer(zx::channel channel, fxl::WeakPtr<bt::gap::Adapter> adapter,
             fxl::WeakPtr<bt::gatt::GATT> gatt);
  ~HostServer() override;

  // ::fuchsia::bluetooth::host::Host overrides:
  void WatchState(WatchStateCallback callback) override;
  void SetLocalData(::fuchsia::bluetooth::sys::HostData host_data) override;
  void WatchPeers(WatchPeersCallback callback) override;
  void RestoreBonds(::std::vector<fuchsia::bluetooth::sys::BondingData> bonds,
                    RestoreBondsCallback callback) override;
  void SetLocalName(::std::string local_name, SetLocalNameCallback callback) override;
  void SetDeviceClass(fuchsia::bluetooth::DeviceClass device_class,
                      SetDeviceClassCallback callback) override;

  void StartDiscovery(StartDiscoveryCallback callback) override;
  void StopDiscovery() override;
  void SetConnectable(bool connectable, SetConnectableCallback callback) override;
  void SetDiscoverable(bool discoverable, SetDiscoverableCallback callback) override;
  void EnableBackgroundScan(bool enabled) override;
  void EnablePrivacy(bool enabled) override;
  void SetLeSecurityMode(::fuchsia::bluetooth::sys::LeSecurityMode mode) override;
  void SetPairingDelegate(
      ::fuchsia::bluetooth::sys::InputCapability input,
      ::fuchsia::bluetooth::sys::OutputCapability output,
      ::fidl::InterfaceHandle<::fuchsia::bluetooth::sys::PairingDelegate> delegate) override;
  void Connect(::fuchsia::bluetooth::PeerId id, ConnectCallback callback) override;
  void Disconnect(::fuchsia::bluetooth::PeerId id, DisconnectCallback callback) override;
  void Pair(::fuchsia::bluetooth::PeerId id, ::fuchsia::bluetooth::sys::PairingOptions options,
            PairCallback callback) override;
  void Forget(::fuchsia::bluetooth::PeerId id, ForgetCallback callback) override;

  void RequestLowEnergyCentral(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Central> central) override;
  void RequestLowEnergyPeripheral(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Peripheral> peripheral) override;
  void RequestGattServer(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> server) override;
  void RequestGatt2Server(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::Server> server) override;
  void RequestProfile(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::bredr::Profile> profile) override;
  void Close() override;

 private:
  // bt::gap::PairingDelegate overrides:
  bt::sm::IOCapability io_capability() const override;
  void CompletePairing(bt::PeerId id, bt::sm::Result<> status) override;
  void ConfirmPairing(bt::PeerId id, ConfirmCallback confirm) override;
  void DisplayPasskey(bt::PeerId id, uint32_t passkey, DisplayMethod method,
                      ConfirmCallback confirm) override;
  void RequestPasskey(bt::PeerId id, PasskeyResponseCallback respond) override;

  // Common code used for showing a user intent (except passkey request).
  void DisplayPairingRequest(bt::PeerId id, std::optional<uint32_t> passkey,
                             fuchsia::bluetooth::sys::PairingMethod method,
                             ConfirmCallback confirm);

  // Called by |adapter()->peer_cache()| when a peer is updated.
  void OnPeerUpdated(const bt::gap::Peer& peer);

  // Called by |adapter()->peer_cache()| when a peer is removed.
  void OnPeerRemoved(bt::PeerId identifier);

  // Called by |adapter()->peer_cache()| when a peer is bonded.
  void OnPeerBonded(const bt::gap::Peer& peer);

  void ConnectLowEnergy(bt::PeerId id, ConnectCallback callback);
  void ConnectBrEdr(bt::PeerId peer_id, ConnectCallback callback);

  void PairLowEnergy(bt::PeerId id, ::fuchsia::bluetooth::sys::PairingOptions options,
                     PairCallback callback);
  void PairBrEdr(bt::PeerId id, PairCallback callback);
  // Called when a connection is established to a peer, either when initiated
  // by a user via a client of Host.fidl, or automatically by the GAP adapter
  void RegisterLowEnergyConnection(std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_ref,
                                   bool auto_connect);

  // Called when |server| receives a channel connection error.
  void OnConnectionError(Server* server);

  // Helper to start LE Discovery (called by StartDiscovery)
  void StartLEDiscovery(StartDiscoveryCallback callback);

  // Resets the I/O capability of this server to no I/O and tells the GAP layer
  // to reject incoming pairing requests.
  void ResetPairingDelegate();

  // Resolves any HostInfo watcher with the current adapter state.
  void NotifyInfoChange();

  // Helper for binding a fidl::InterfaceRequest to a FIDL server of type
  // ServerType.
  template <typename ServerType, typename... Args>
  void BindServer(Args... args) {
    auto server = std::make_unique<ServerType>(adapter()->AsWeakPtr(), std::move(args)...);
    Server* s = server.get();
    server->set_error_handler([this, s](zx_status_t status) { this->OnConnectionError(s); });
    servers_[server.get()] = std::move(server);
  }

  fuchsia::bluetooth::sys::PairingDelegatePtr pairing_delegate_;

  // We hold a weak pointer to GATT for dispatching GATT FIDL requests.
  fxl::WeakPtr<bt::gatt::GATT> gatt_;

  bool requesting_discovery_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> le_discovery_session_;
  std::unique_ptr<bt::gap::BrEdrDiscoverySession> bredr_discovery_session_;

  bool requesting_background_scan_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> le_background_scan_;

  bool requesting_discoverable_;
  std::unique_ptr<bt::gap::BrEdrDiscoverableSession> bredr_discoverable_session_;

  bt::sm::IOCapability io_capability_;

  // All active FIDL interface servers.
  // NOTE: Each key is a raw pointer that is owned by the corresponding value.
  // This allows us to create a set of managed objects that can be looked up via
  // raw pointer.
  std::unordered_map<Server*, std::unique_ptr<Server>> servers_;

  // All LE connections that were either initiated by this HostServer or
  // auto-connected by the system.
  // TODO(armansito): Consider storing auto-connected references separately from
  // directly connected references.
  std::unordered_map<bt::PeerId, std::unique_ptr<bt::gap::LowEnergyConnectionHandle>>
      le_connections_;

  // Used to drive the WatchState() method.
  bt_lib_fidl::HangingGetter<fuchsia::bluetooth::sys::HostInfo> info_getter_;

  // Used to drive the WatchPeers() method.
  WatchPeersGetter watch_peers_getter_;

  // Id of the PeerCache::add_peer_updated_callback callback. Used to remove the callback when this
  // server is closed.
  bt::gap::PeerCache::CallbackId peer_updated_callback_id_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<HostServer> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_
