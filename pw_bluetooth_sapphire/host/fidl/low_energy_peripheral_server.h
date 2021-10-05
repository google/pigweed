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
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the low_energy::Peripheral FIDL interface.
class LowEnergyPeripheralServer : public AdapterServerBase<fuchsia::bluetooth::le::Peripheral> {
 public:
  LowEnergyPeripheralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                            fidl::InterfaceRequest<fuchsia::bluetooth::le::Peripheral> request);
  ~LowEnergyPeripheralServer() override;

  // fuchsia::bluetooth::le::Peripheral overrides:
  void Advertise(
      fuchsia::bluetooth::le::AdvertisingParameters parameters,
      fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> advertised_peripheral,
      AdvertiseCallback callback) override;
  void StartAdvertising(fuchsia::bluetooth::le::AdvertisingParameters parameters,
                        ::fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> token,
                        StartAdvertisingCallback callback) override;

  // Returns the connection handle associated with the given |id|, or nullptr if the peer with
  // |id| is no longer connected. Should only be used for testing.
  const bt::gap::LowEnergyConnectionHandle* FindConnectionForTesting(bt::PeerId id) const;

 private:
  using ConnectionRefPtr = std::unique_ptr<bt::gap::LowEnergyConnectionHandle>;
  using AdvertisementInstanceId = uint64_t;
  using ConnectionServerId = uint64_t;

  // Manages state associated with a single invocation of the `Peripheral.Advertise` method.
  class AdvertisementInstance final {
   public:
    using AdvertiseCompleteCallback =
        fit::callback<void(fuchsia::bluetooth::le::Peripheral_Advertise_Result)>;

    // |complete_cb| will be called to send a Peripheral.Advertise response to the client when an
    // error occurs or this AdvertisementInstance is destroyed. This is done so that the FIDL client
    // can determine when the server has terminated this AdvertisementInstance (this is useful for
    // reconfiguring an advertisement).
    AdvertisementInstance(
        LowEnergyPeripheralServer* peripheral_server, AdvertisementInstanceId id,
        fuchsia::bluetooth::le::AdvertisingParameters parameters,
        fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> handle,
        AdvertiseCompleteCallback complete_cb);
    ~AdvertisementInstance();

    // This method is separate from the constructor because HCI-level advertising may be
    // started many times over the life of this object.
    void StartAdvertising();

    // Called when a central connects to us.  When this is called, the
    // advertisement in |advertisement_id| has been stopped.
    void OnConnected(bt::gap::AdvertisementId advertisement_id,
                     bt::gap::Adapter::LowEnergy::ConnectionResult result);

   private:
    // After advertising successfully starts, the advertisement instance must be registered to tie
    // advertising to the lifetime of this object.
    void Register(bt::gap::AdvertisementInstance instance);

    // End the advertisement with a result. Idempotent.
    // This object should be destroyed immediately after calling this method.
    void CloseWith(fpromise::result<void, fuchsia::bluetooth::le::PeripheralError> result);

    LowEnergyPeripheralServer* peripheral_server_;
    AdvertisementInstanceId id_;
    fuchsia::bluetooth::le::AdvertisingParameters parameters_;

    // The advertising handle set by Register. When destroyed, advertising will be stopped.
    std::optional<bt::gap::AdvertisementInstance> instance_;

    // The AdvertisedPeripheral protocol representing this advertisement.
    fidl::InterfacePtr<fuchsia::bluetooth::le::AdvertisedPeripheral> advertised_peripheral_;

    // Callback used to send a response to the Advertise request that started this advertisement.
    AdvertiseCompleteCallback advertise_complete_cb_;

    fxl::WeakPtrFactory<AdvertisementInstance> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisementInstance);
  };

  class AdvertisementInstanceDeprecated final {
   public:
    explicit AdvertisementInstanceDeprecated(
        fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> handle);
    ~AdvertisementInstanceDeprecated();

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

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisementInstanceDeprecated);
  };

  // Called when a central connects to us.  When this is called, the
  // advertisement in |advertisement_id| has been stopped.
  void OnConnectedDeprecated(bt::gap::AdvertisementId advertisement_id,
                             bt::gap::Adapter::LowEnergy::ConnectionResult result);

  // Sets up a Connection server and returns the client end.
  fidl::InterfaceHandle<fuchsia::bluetooth::le::Connection> CreateConnectionServer(
      std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection);

  // Common advertising initiation code shared by Peripheral.{Advertise, StartAdvertising}.
  // If advertising was initiated by `Advertise`, `advertisement_instance` must be set to the
  // identifier of the `AdvertisementInstance` that connections to this advertisement should be
  // routed to. Otherwise, connections will be sent in a `Peripheral.OnConnected` event.
  void StartAdvertisingInternal(
      fuchsia::bluetooth::le::AdvertisingParameters& parameters,
      bt::gap::Adapter::LowEnergy::AdvertisingStatusCallback status_cb,
      std::optional<AdvertisementInstanceId> advertisement_instance = std::nullopt);

  void RemoveAdvertisingInstance(AdvertisementInstanceId id) { advertisements_.erase(id); }

  // Represents the current advertising instance:
  // - Contains no value if advertising was never requested.
  // - Contains a value while advertising is being (re)enabled and during advertising.
  // - May correspond to an invalidated advertising instance if advertising is stopped by closing
  //   the AdvertisingHandle.
  std::optional<AdvertisementInstanceDeprecated> advertisement_deprecated_;

  // Map of all active advertisement instances associated with a call to `Advertise`.
  // bt::gap::AdvertisementId cannot be used as a map key because it is received asynchronously, and
  // we need an advertisement ID to refer to before advertising starts.
  // TODO(fxbug.dev/77644): Support AdvertisedPeripheral protocols that outlive this Peripheral
  // protocol. This may require passing AdvertisementInstances to HostServer.
  AdvertisementInstanceId next_advertisement_instance_id_ = 0u;
  std::unordered_map<AdvertisementInstanceId, AdvertisementInstance> advertisements_;

  // Connections that were initiated to this peripheral. A single Peripheral instance can hold
  // many connections across numerous advertisements that it initiates during its lifetime.
  ConnectionServerId next_connection_server_id_ = 0u;
  std::unordered_map<ConnectionServerId, std::unique_ptr<LowEnergyConnectionServer>> connections_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyPeripheralServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyPeripheralServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_PERIPHERAL_SERVER_H_
