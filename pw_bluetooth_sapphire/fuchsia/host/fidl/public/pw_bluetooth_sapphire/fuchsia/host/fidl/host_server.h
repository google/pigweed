// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <fuchsia/bluetooth/host/cpp/fidl.h>
#include <lib/zx/channel.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "lib/fidl/cpp/interface_request.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/fuchsia/lib/fidl/hanging_getter.h"
#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/adapter.h"
#include "pw_bluetooth_sapphire/internal/host/gap/bredr_connection_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/bredr_discovery_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_discovery_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/pairing_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bthost {

// Implements the Host FIDL interface. Owns all FIDL connections that have been
// opened through it.
class HostServer : public AdapterServerBase<fuchsia::bluetooth::host::Host>,
                   public bt::gap::PairingDelegate {
 public:
  HostServer(zx::channel channel,
             const bt::gap::Adapter::WeakPtr& adapter,
             bt::gatt::GATT::WeakPtr gatt);
  ~HostServer() override;

  // ::fuchsia::bluetooth::host::Host overrides:
  void RequestProtocol(
      ::fuchsia::bluetooth::host::ProtocolRequest request) override;
  void WatchState(WatchStateCallback callback) override;
  void SetLocalData(::fuchsia::bluetooth::sys::HostData host_data) override;
  void SetPeerWatcher(
      ::fidl::InterfaceRequest<::fuchsia::bluetooth::host::PeerWatcher>
          peer_watcher) override;
  void SetLocalName(::std::string local_name,
                    SetLocalNameCallback callback) override;
  void SetDeviceClass(fuchsia::bluetooth::DeviceClass device_class,
                      SetDeviceClassCallback callback) override;

  void StartDiscovery(
      ::fuchsia::bluetooth::host::HostStartDiscoveryRequest request) override;
  void SetConnectable(bool connectable,
                      SetConnectableCallback callback) override;
  void SetDiscoverable(bool discoverable,
                       SetDiscoverableCallback callback) override;
  void EnableBackgroundScan(bool enabled) override;
  void EnablePrivacy(bool enabled) override;
  void SetBrEdrSecurityMode(
      ::fuchsia::bluetooth::sys::BrEdrSecurityMode mode) override;
  void SetLeSecurityMode(
      ::fuchsia::bluetooth::sys::LeSecurityMode mode) override;
  void SetPairingDelegate(
      ::fuchsia::bluetooth::sys::InputCapability input,
      ::fuchsia::bluetooth::sys::OutputCapability output,
      ::fidl::InterfaceHandle<::fuchsia::bluetooth::sys::PairingDelegate>
          delegate) override;
  void Connect(::fuchsia::bluetooth::PeerId id,
               ConnectCallback callback) override;
  void Disconnect(::fuchsia::bluetooth::PeerId id,
                  DisconnectCallback callback) override;
  void Pair(::fuchsia::bluetooth::PeerId id,
            ::fuchsia::bluetooth::sys::PairingOptions options,
            PairCallback callback) override;
  void Forget(::fuchsia::bluetooth::PeerId id,
              ForgetCallback callback) override;
  void Shutdown() override;
  void SetBondingDelegate(
      ::fidl::InterfaceRequest<::fuchsia::bluetooth::host::BondingDelegate>
          request) override;
  void handle_unknown_method(uint64_t ordinal,
                             bool method_has_response) override;

 private:
  class DiscoverySessionServer
      : public ServerBase<::fuchsia::bluetooth::host::DiscoverySession> {
   public:
    explicit DiscoverySessionServer(
        fidl::InterfaceRequest<::fuchsia::bluetooth::host::DiscoverySession>
            request,
        HostServer* host);

    void Close(zx_status_t epitaph) { binding()->Close(epitaph); }

    // ::fuchsia::bluetooth::host::Discovery overrides:
    void Stop() override;

   private:
    void handle_unknown_method(uint64_t ordinal,
                               bool method_has_response) override;

    HostServer* host_;
  };

  class PeerWatcherServer
      : public ServerBase<::fuchsia::bluetooth::host::PeerWatcher> {
   public:
    PeerWatcherServer(::fidl::InterfaceRequest<
                          ::fuchsia::bluetooth::host::PeerWatcher> request,
                      bt::gap::PeerCache* peer_cache,
                      HostServer* host);
    ~PeerWatcherServer() override;

    // Called by |adapter()->peer_cache()| when a peer is updated.
    void OnPeerUpdated(const bt::gap::Peer& peer);

    // Called by |adapter()->peer_cache()| when a peer is removed.
    void OnPeerRemoved(bt::PeerId identifier);

    void MaybeCallCallback();

   private:
    using Updated = std::vector<fuchsia::bluetooth::sys::Peer>;
    using Removed = std::vector<fuchsia::bluetooth::PeerId>;

    // PeerWatcher overrides:
    void GetNext(::fuchsia::bluetooth::host::PeerWatcher::GetNextCallback
                     callback) override;
    void handle_unknown_method(uint64_t ordinal,
                               bool method_has_response) override;

    std::unordered_set<bt::PeerId> updated_;
    std::unordered_set<bt::PeerId> removed_;

    bt::gap::PeerCache* peer_cache_;
    // Id of the PeerCache::add_peer_updated_callback callback. Used to remove
    // the callback when this server is closed.
    bt::gap::PeerCache::CallbackId peer_updated_callback_id_;

    ::fuchsia::bluetooth::host::PeerWatcher::GetNextCallback callback_ =
        nullptr;

    HostServer* host_;

    // Keep this as the last member to make sure that all weak pointers are
    // invalidated before other members get destroyed.
    WeakSelf<PeerWatcherServer> weak_self_;
  };

  class BondingDelegateServer
      : public ServerBase<::fuchsia::bluetooth::host::BondingDelegate> {
   public:
    explicit BondingDelegateServer(
        ::fidl::InterfaceRequest<::fuchsia::bluetooth::host::BondingDelegate>
            request,
        HostServer* host);

    void OnNewBondingData(const bt::gap::Peer& peer);

   private:
    // BondingDelegate overrides:
    void RestoreBonds(
        ::std::vector<::fuchsia::bluetooth::sys::BondingData> bonds,
        RestoreBondsCallback callback) override;
    void WatchBonds(WatchBondsCallback callback) override;
    void handle_unknown_method(uint64_t ordinal,
                               bool method_has_response) override;

    void MaybeNotifyWatchBonds();

    HostServer* host_;
    // Queued bond updates that will be sent on the next call to WatchBonds.
    std::queue<::fuchsia::bluetooth::sys::BondingData> updated_;
    fit::callback<void(
        ::fuchsia::bluetooth::host::BondingDelegate_WatchBonds_Result)>
        watch_bonds_cb_;
  };

  // bt::gap::PairingDelegate overrides:
  bt::sm::IOCapability io_capability() const override;
  void CompletePairing(bt::PeerId id, bt::sm::Result<> status) override;
  void ConfirmPairing(bt::PeerId id, ConfirmCallback confirm) override;
  void DisplayPasskey(bt::PeerId id,
                      uint32_t passkey,
                      DisplayMethod method,
                      ConfirmCallback confirm) override;
  void RequestPasskey(bt::PeerId id, PasskeyResponseCallback respond) override;

  // Common code used for showing a user intent (except passkey request).
  void DisplayPairingRequest(bt::PeerId id,
                             std::optional<uint32_t> passkey,
                             fuchsia::bluetooth::sys::PairingMethod method,
                             ConfirmCallback confirm);

  // Called by |adapter()->peer_cache()| when a peer is bonded.
  void OnPeerBonded(const bt::gap::Peer& peer);

  void ConnectLowEnergy(bt::PeerId id, ConnectCallback callback);
  void ConnectBrEdr(bt::PeerId peer_id, ConnectCallback callback);

  void PairLowEnergy(bt::PeerId id,
                     ::fuchsia::bluetooth::sys::PairingOptions options,
                     PairCallback callback);
  void PairBrEdr(bt::PeerId id, PairCallback callback);
  // Called when a connection is established to a peer, either when initiated
  // by a user via a client of Host.fidl, or automatically by the GAP adapter
  void RegisterLowEnergyConnection(
      std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_ref,
      bool auto_connect);

  // Called when |server| receives a channel connection error.
  void OnConnectionError(Server* server);

  // Helper to start LE Discovery (called by StartDiscovery)
  void StartLEDiscovery();

  void StopDiscovery(zx_status_t epitaph, bool notify_info_change = true);

  void OnDiscoverySessionServerClose(DiscoverySessionServer* server);

  // Resets the I/O capability of this server to no I/O and tells the GAP layer
  // to reject incoming pairing requests.
  void ResetPairingDelegate();

  // Resolves any HostInfo watcher with the current adapter state.
  void NotifyInfoChange();

  void RestoreBonds(
      ::std::vector<::fuchsia::bluetooth::sys::BondingData> bonds,
      ::fuchsia::bluetooth::host::BondingDelegate::RestoreBondsCallback
          callback);

  // Helper for binding a fidl::InterfaceRequest to a FIDL server of type
  // ServerType.
  template <typename ServerType, typename... Args>
  void BindServer(Args... args) {
    auto server = std::make_unique<ServerType>(std::move(args)...);
    Server* s = server.get();
    server->set_error_handler(
        [this, s](zx_status_t status) { this->OnConnectionError(s); });
    servers_[server.get()] = std::move(server);
  }

  fuchsia::bluetooth::sys::PairingDelegatePtr pairing_delegate_;

  // We hold a weak pointer to GATT for dispatching GATT FIDL requests.
  bt::gatt::GATT::WeakPtr gatt_;

  std::unordered_map<DiscoverySessionServer*,
                     std::unique_ptr<DiscoverySessionServer>>
      discovery_session_servers_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> le_discovery_session_;
  std::unique_ptr<bt::gap::BrEdrDiscoverySession> bredr_discovery_session_;

  bool requesting_background_scan_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> le_background_scan_;

  bool requesting_discoverable_;
  std::unique_ptr<bt::gap::BrEdrDiscoverableSession>
      bredr_discoverable_session_;

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
  std::unordered_map<bt::PeerId,
                     std::unique_ptr<bt::gap::LowEnergyConnectionHandle>>
      le_connections_;

  // Used to drive the WatchState() method.
  bt_lib_fidl::HangingGetter<fuchsia::bluetooth::sys::HostInfo> info_getter_;

  std::optional<PeerWatcherServer> peer_watcher_server_;

  std::optional<BondingDelegateServer> bonding_delegate_server_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  WeakSelf<HostServer> weak_self_;

  WeakSelf<PairingDelegate> weak_pairing_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostServer);
};

}  // namespace bthost
