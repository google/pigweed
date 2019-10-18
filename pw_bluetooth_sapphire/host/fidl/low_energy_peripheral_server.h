// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_

#include <fuchsia/bluetooth/le/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <memory>
#include <unordered_map>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/fidl/low_energy_connection_server.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the low_energy::Central FIDL interface.
class LowEnergyPeripheralServer : public AdapterServerBase<fuchsia::bluetooth::le::Peripheral> {
 public:
  LowEnergyPeripheralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                            fidl::InterfaceRequest<fuchsia::bluetooth::le::Peripheral> request);
  ~LowEnergyPeripheralServer() override;

  // fuchsia::bluetooth::le::Peripheral overrides:
  void StartAdvertising(fuchsia::bluetooth::le::AdvertisingParameters parameters,
                        ::fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> token,
                        StartAdvertisingCallback callback) override;
  void StartAdvertisingDeprecated(
      fuchsia::bluetooth::le::AdvertisingDataDeprecated advertising_data,
      fuchsia::bluetooth::le::AdvertisingDataDeprecatedPtr scan_result, bool connectable,
      uint32_t interval, bool anonymous, StartAdvertisingDeprecatedCallback callback) override;

  void StopAdvertisingDeprecated(::std::string advertisement_id,
                                 StopAdvertisingDeprecatedCallback callback) override;

 private:
  using ConnectionRefPtr = bt::gap::LowEnergyConnectionRefPtr;

  // DEPRECATED
  class InstanceData final {
   public:
    InstanceData() = default;
    InstanceData(bt::gap::AdvertisementInstance instance,
                 fxl::WeakPtr<LowEnergyPeripheralServer> owner);

    InstanceData(InstanceData&& other) = default;
    InstanceData& operator=(InstanceData&& other) = default;

    bool connectable() const { return static_cast<bool>(owner_->binding()); }

    // Takes ownership of |conn_ref| and notifies the delegate of the new
    // connection.
    void RetainConnection(ConnectionRefPtr conn_ref, fuchsia::bluetooth::le::RemoteDevice central);

    // Deletes the connection reference and notifies the delegate of
    // disconnection.
    void ReleaseConnection();

   private:
    bt::gap::AdvertisementInstance instance_;
    ConnectionRefPtr conn_ref_;
    // The object that created and owns this InstanceData.
    // |owner_| must outlive the InstanceData.
    fxl::WeakPtr<LowEnergyPeripheralServer> owner_;  // weak

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(InstanceData);
  };

  class AdvertisementInstance final {
   public:
    explicit AdvertisementInstance(
        fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> handle);
    ~AdvertisementInstance();

    // Begin watching for ZX_CHANNEL_PEER_CLOSED events on the AdvertisingHandle this was
    // initialized with. The returned status will indicate an error if wait cannot be initiated
    // (e.g. because the peer closed its end of the channel).
    zx_status_t Register(bt::gap::AdvertisementInstance instance);

    // Returns the ID assigned to this instance, or bt::gap::kInvalidAdvertisementId if one wasn't
    // assigned.
    bt::gap::AdvertisementId id() const {
      return instance_ ? instance_->id() : bt::gap::kInvalidAdvertisementId;
    }

   private:
    std::optional<bt::gap::AdvertisementInstance> instance_;
    fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> handle_;
    async::Wait handle_closed_wait_;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisementInstance);
  };

  bool StopAdvertisingInternal(bt::gap::AdvertisementId id);

  // Called when a central connects to us.  When this is called, the
  // advertisement in |advertisement_id| has been stopped.
  void OnConnected(bt::gap::AdvertisementId advertisement_id, bt::hci::ConnectionPtr link);
  void OnConnectedDeprecated(bt::gap::AdvertisementId advertisement_id,
                             bt::hci::ConnectionPtr link);

  // Represents the current advertising instance:
  // - Contains no value if advertising was never requested.
  // - Contains a value while advertising is being (re)enabled and during advertising.
  // - May correspond to an invalidated advertising instance if advertising is stopped by closing
  //   the AdvertisingHandle.
  std::optional<AdvertisementInstance> advertisement_;

  // Connections that were initiated to this peripheral. A single Peripheral instance can hold many
  // connections across numerous advertisements that it initiates during its lifetime (although
  // there is at most one active advertisement at a time).
  std::unordered_map<bt::gap::AdvertisementId, std::unique_ptr<LowEnergyConnectionServer>>
      connections_;

  // Tracks currently active advertisements. DEPRECATED
  std::unordered_map<bt::gap::AdvertisementId, InstanceData> instances_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyPeripheralServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyPeripheralServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_
