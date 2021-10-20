// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CENTRAL_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CENTRAL_SERVER_H_

#include <fuchsia/bluetooth/le/cpp/fidl.h>

#include <memory>
#include <unordered_map>

#include <fbl/macros.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/gatt_client_server.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"

namespace bthost {

// Implements the low_energy::Central FIDL interface.
class LowEnergyCentralServer : public AdapterServerBase<fuchsia::bluetooth::le::Central> {
 public:
  // The maximum number of peers that will be queued for a ScanResultWatcher.Watch call.
  // This hard limit prevents unbounded memory usage for unresponsive clients. The value is mostly
  // arbitrary, as queued `PeerId`s are small and peak memory usage, occurring when creating a
  // vector of FIDL `le.Peer`s, is limited by the size of the FIDL channel.
  constexpr static const size_t kMaxPendingScanResultWatcherPeers = 100;

  LowEnergyCentralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                         ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Central> request,
                         fxl::WeakPtr<bt::gatt::GATT> gatt);
  ~LowEnergyCentralServer() override;

  // Returns the connection pointer in the connections_ map, if it exists. The pointer will be
  // nullptr if a request is pending. Should only be used for testing.
  std::optional<bt::gap::LowEnergyConnectionHandle*> FindConnectionForTesting(
      bt::PeerId identifier);

 private:
  class ScanResultWatcherServer : public ServerBase<fuchsia::bluetooth::le::ScanResultWatcher> {
   public:
    using WatchCallbackOnce = fit::callback<void(std::vector<fuchsia::bluetooth::le::Peer>)>;

    // `error_cb` will be called when the client closes the protocol.
    ScanResultWatcherServer(
        fxl::WeakPtr<bt::gap::Adapter> adapter,
        fidl::InterfaceRequest<fuchsia::bluetooth::le::ScanResultWatcher> watcher,
        fit::callback<void()> error_cb);
    ~ScanResultWatcherServer() override = default;

    // Closes the protocol and sends `epitaph` as the epitaph. Idempotent.
    void Close(zx_status_t epitaph);

    // Queue `peers` to be sent in response to `Watch()`.
    void AddPeers(std::unordered_set<bt::PeerId> peers);

    // fuchsia::bluetooth::le::ScanResultWatcher overrides:
    void Watch(WatchCallback callback) override;

   private:
    // If the client has a pending `Watch()`, send the maximum number of peers that will fit in the
    // channel.
    void MaybeSendPeers();

    fxl::WeakPtr<bt::gap::Adapter> adapter_;
    std::unordered_set<bt::PeerId> updated_peers_;
    WatchCallbackOnce watch_callback_ = nullptr;
    fit::callback<void()> error_callback_;
  };

  // ScanInstance represents a call to `Scan` that has not stopped yet.
  class ScanInstance {
   public:
    using ScanCompleteCallback = fit::callback<void()>;

    ScanInstance(fxl::WeakPtr<bt::gap::Adapter> adapter, LowEnergyCentralServer* central_server,
                 std::vector<fuchsia::bluetooth::le::Filter> filters,
                 fidl::InterfaceRequest<fuchsia::bluetooth::le::ScanResultWatcher> watcher,
                 ScanCallback cb);
    ~ScanInstance();
    // Closes the ScanResultWatcher protocol with the epitaph `status` and sends an empty response
    // to `Scan`. Idempotent.
    void Close(zx_status_t status);

    // Queue peers to be sent to the client via `ScanResultWatcher.Watch`. `peers` will be filtered
    // by the client's `ScanOptions` filters before being sent.
    void FilterAndAddPeers(std::unordered_set<bt::PeerId> peers);

   private:
    std::unique_ptr<bt::gap::LowEnergyDiscoverySession> scan_session_;
    ScanResultWatcherServer result_watcher_;
    // Callback used to send an empty response to the client's `Scan()` call.
    ScanCompleteCallback scan_complete_callback_;
    bt::gap::PeerCache::CallbackId peer_updated_callback_id_;
    // The filters specified in `ScanOptions`.
    std::vector<bt::gap::DiscoveryFilter> filters_;
    LowEnergyCentralServer* central_server_;
    fxl::WeakPtr<bt::gap::Adapter> adapter_;
    fxl::WeakPtrFactory<ScanInstance> weak_ptr_factory_;
  };

  // fuchsia::bluetooth::le::Central overrides:
  void Scan(fuchsia::bluetooth::le::ScanOptions options,
            fidl::InterfaceRequest<fuchsia::bluetooth::le::ScanResultWatcher> result_watcher,
            ScanCallback callback) override;
  void GetPeripherals(::fidl::VectorPtr<::std::string> service_uuids,
                      GetPeripheralsCallback callback) override;
  void GetPeripheral(::std::string identifier, GetPeripheralCallback callback) override;
  void StartScan(fuchsia::bluetooth::le::ScanFilterPtr filter, StartScanCallback callback) override;
  void StopScan() override;
  void ConnectPeripheral(::std::string identifier,
                         fuchsia::bluetooth::le::ConnectionOptions connection_options,
                         ::fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Client> client_request,
                         ConnectPeripheralCallback callback) override;
  void DisconnectPeripheral(::std::string identifier,
                            DisconnectPeripheralCallback callback) override;

  // Called by |scan_session_| when a device is discovered.
  void OnScanResult(const bt::gap::Peer& peer);

  // Notifies the delegate that the scan state for this Central has changed.
  void NotifyScanStateChanged(bool scanning);

  // Notifies the delegate that the device with the given identifier has been
  // disconnected.
  void NotifyPeripheralDisconnected(bt::PeerId peer_id);

  void ClearScan() { scan_instance_.reset(); }

  // GATT is used to construct GattClientServers upon connection.
  fxl::WeakPtr<bt::gatt::GATT> gatt_;

  // Stores active GATT client FIDL servers. Only 1 client server per peer may exist.
  std::unordered_map<bt::PeerId, std::unique_ptr<GattClientServer>> gatt_client_servers_;

  // The currently active LE discovery session. This is initialized when a
  // client requests to perform a scan.
  bool requesting_scan_deprecated_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> scan_session_deprecated_;

  std::unique_ptr<ScanInstance> scan_instance_;

  // This client's connection references. A client can hold a connection to
  // multiple peers. Each key is a peer identifier. Each value is
  //   a. nullptr, if a connect request to this device is currently pending.
  //   b. a valid reference if this Central is holding a connection reference to
  //   this device.
  std::unordered_map<bt::PeerId, std::unique_ptr<bt::gap::LowEnergyConnectionHandle>> connections_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyCentralServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyCentralServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CENTRAL_SERVER_H_
