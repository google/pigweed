// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_

#include <fbl/macros.h>
#include <fuchsia/bluetooth/le/cpp/fidl.h>

#include <memory>
#include <unordered_map>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the low_energy::Central FIDL interface.
class LowEnergyPeripheralServer
    : public AdapterServerBase<fuchsia::bluetooth::le::Peripheral> {
 public:
  LowEnergyPeripheralServer(
      fxl::WeakPtr<bt::gap::Adapter> adapter,
      fidl::InterfaceRequest<fuchsia::bluetooth::le::Peripheral> request);
  ~LowEnergyPeripheralServer() override;

 private:
  using ConnectionRefPtr = bt::gap::LowEnergyConnectionRefPtr;

  class InstanceData final {
   public:
    InstanceData() = default;
    InstanceData(bt::gap::AdvertisementId id,
                 fxl::WeakPtr<LowEnergyPeripheralServer> owner);

    InstanceData(InstanceData&& other) = default;
    InstanceData& operator=(InstanceData&& other) = default;

    bool connectable() const { return static_cast<bool>(owner_->binding()); }

    // Takes ownership of |conn_ref| and notifies the delegate of the new
    // connection.
    void RetainConnection(ConnectionRefPtr conn_ref,
                          fuchsia::bluetooth::le::RemoteDevice central);

    // Deletes the connection reference and notifies the delegate of
    // disconnection.
    void ReleaseConnection();

   private:
    bt::gap::AdvertisementId id_;
    ConnectionRefPtr conn_ref_;
    // The object that created and owns this InstanceData.
    // |owner_| must outlive the InstanceData.
    fxl::WeakPtr<LowEnergyPeripheralServer> owner_;  // weak

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(InstanceData);
  };

  // fuchsia::bluetooth::le::Peripheral overrides:
  void StartAdvertising(
      fuchsia::bluetooth::le::AdvertisingData advertising_data,
      fuchsia::bluetooth::le::AdvertisingDataPtr scan_result, bool connectable,
      uint32_t interval, bool anonymous,
      StartAdvertisingCallback callback) override;

  void StopAdvertising(::std::string advertisement_id,
                       StopAdvertisingCallback callback) override;
  bool StopAdvertisingInternal(bt::gap::AdvertisementId id);

  // Called when a central connects to us.  When this is called, the
  // advertisement in |advertisement_id| has been stopped.
  void OnConnected(bt::gap::AdvertisementId advertisement_id,
                   bt::hci::ConnectionPtr link);

  // Tracks currently active advertisements.
  std::unordered_map<bt::gap::AdvertisementId, InstanceData> instances_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyPeripheralServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyPeripheralServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_
