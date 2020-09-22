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
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt_host.h"

namespace bthost {

// Implements the low_energy::Central FIDL interface.
class LowEnergyCentralServer : public AdapterServerBase<fuchsia::bluetooth::le::Central> {
 public:
  LowEnergyCentralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                         ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Central> request,
                         fxl::WeakPtr<GattHost> gatt_host);
  ~LowEnergyCentralServer() override;

  // Returns the connection pointer in the connections_ map, if it exists. The pointer will be
  // nullptr if a request is pending. Should only be used for testing.
  std::optional<bt::gap::LowEnergyConnectionRef*> FindConnectionForTesting(bt::PeerId identifier);

 private:
  // fuchsia::bluetooth::le::Central overrides:
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

  // The GATT host is used to instantiate GATT Clients upon connection.
  fxl::WeakPtr<GattHost> gatt_host_;

  // The currently active LE discovery session. This is initialized when a
  // client requests to perform a scan.
  bool requesting_scan_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> scan_session_;

  // This client's connection references. A client can hold a connection to
  // multiple peers. Each key is a peer identifier. Each value is
  //   a. nullptr, if a connect request to this device is currently pending.
  //   b. a valid reference if this Central is holding a connection reference to
  //   this device.
  std::unordered_map<bt::PeerId, bt::gap::LowEnergyConnectionRefPtr> connections_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyCentralServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyCentralServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CENTRAL_SERVER_H_
