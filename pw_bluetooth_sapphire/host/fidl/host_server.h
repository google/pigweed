// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_

#include <memory>
#include <unordered_map>

#include <fuchsia/bluetooth/control/cpp/fidl.h>
#include <fuchsia/bluetooth/host/cpp/fidl.h>
#include <lib/zx/channel.h>

#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "src/lib/fxl/macros.h"
#include "src/lib/fxl/memory/weak_ptr.h"

#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_delegate.h"

namespace bthost {

class GattHost;

// Implements the Host FIDL interface. Owns all FIDL connections that have been
// opened through it.
class HostServer : public AdapterServerBase<fuchsia::bluetooth::host::Host>,
                   public bt::gap::PairingDelegate {
 public:
  HostServer(zx::channel channel, fxl::WeakPtr<bt::gap::Adapter> adapter,
             fbl::RefPtr<GattHost> gatt_host);
  ~HostServer() override;

 private:
  // ::fuchsia::bluetooth::Host overrides:
  void GetInfo(GetInfoCallback callback) override;
  void ListDevices(ListDevicesCallback callback) override;
  void AddBondedDevices(
      ::std::vector<fuchsia::bluetooth::host::BondingData> bonds,
      AddBondedDevicesCallback callback) override;
  void SetLocalName(::std::string local_name,
                    SetLocalNameCallback callback) override;

  void StartDiscovery(StartDiscoveryCallback callback) override;
  void StopDiscovery(StopDiscoveryCallback callback) override;
  void SetConnectable(bool connectable,
                      SetConnectableCallback callback) override;
  void SetDiscoverable(bool discoverable,
                       SetDiscoverableCallback callback) override;
  void EnableBackgroundScan(bool enabled) override;
  void SetPairingDelegate(
      ::fuchsia::bluetooth::control::InputCapabilityType input,
      ::fuchsia::bluetooth::control::OutputCapabilityType output,
      ::fidl::InterfaceHandle<::fuchsia::bluetooth::control::PairingDelegate>
          delegate) override;
  void Connect(::std::string device_id, ConnectCallback callback) override;

  void RequestLowEnergyCentral(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Central> central)
      override;
  void RequestLowEnergyPeripheral(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::le::Peripheral> peripheral)
      override;
  void RequestGattServer(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> server)
      override;
  void RequestProfile(
      ::fidl::InterfaceRequest<fuchsia::bluetooth::bredr::Profile> profile)
      override;
  void Close() override;

  // bt::gap::PairingDelegate overrides:
  bt::sm::IOCapability io_capability() const override;
  void CompletePairing(bt::gap::DeviceId id, bt::sm::Status status) override;
  void ConfirmPairing(bt::gap::DeviceId id, ConfirmCallback confirm) override;
  void DisplayPasskey(bt::gap::DeviceId id, uint32_t passkey,
                      ConfirmCallback confirm) override;
  void RequestPasskey(bt::gap::DeviceId id,
                      PasskeyResponseCallback respond) override;

  // Called by |adapter()->remote_device_cache()| when a remote device is
  // updated.
  void OnRemoteDeviceUpdated(const bt::gap::RemoteDevice& remote_device);

  // Called by |adapter()->remote_device_cache()| when a remote device is
  // removed.
  void OnRemoteDeviceRemoved(bt::gap::DeviceId identifier);

  // Called by |adapter()->remote_device_cache()| when a remote device is
  // bonded.
  void OnRemoteDeviceBonded(const bt::gap::RemoteDevice& remote_device);

  // Called when a connection is established to a remote device, either when
  // initiated by a user via a client of Host.fidl, or automatically by the GAP
  // adapter
  void OnConnect(bt::gap::LowEnergyConnectionRefPtr conn_ref,
                 bool auto_connect);

  // Called when |server| receives a channel connection error.
  void OnConnectionError(Server* server);

  // Helper to start LE Discovery (called by StartDiscovery)
  void StartLEDiscovery(StartDiscoveryCallback callback);

  // Resets the I/O capability of this server to no I/O and tells the GAP layer
  // to reject incoming pairing requests.
  void ResetPairingDelegate();

  // Helper for binding a fidl::InterfaceRequest to a FIDL server of type
  // ServerType.
  template <typename ServerType, typename... Args>
  void BindServer(Args... args) {
    auto server = std::make_unique<ServerType>(adapter()->AsWeakPtr(),
                                               std::move(args)...);
    Server* s = server.get();
    server->set_error_handler(
        [this, s](zx_status_t status) { this->OnConnectionError(s); });
    servers_[server.get()] = std::move(server);
  }

  fuchsia::bluetooth::control::PairingDelegatePtr pairing_delegate_;

  // We hold a reference to GattHost for dispatching GATT FIDL requests.
  fbl::RefPtr<GattHost> gatt_host_;

  bool requesting_discovery_;
  std::unique_ptr<bt::gap::LowEnergyDiscoverySession> le_discovery_session_;
  std::unique_ptr<bt::gap::BrEdrDiscoverySession> bredr_discovery_session_;

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
  std::unordered_map<bt::gap::DeviceId, bt::gap::LowEnergyConnectionRefPtr>
      le_connections_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<HostServer> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(HostServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HOST_SERVER_H_
