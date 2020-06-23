// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/sys/inspect/cpp/component.h>
#include <lib/zx/vmo.h>
#include <zircon/assert.h>

#include <memory>
#include <string>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter_state.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bonding_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/server.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {

namespace hci {
class LowEnergyAdvertiser;
class LowEnergyConnector;
class LowEnergyScanner;
class SequentialCommandRunner;
class Transport;
}  // namespace hci

namespace gap {

class BrEdrConnectionManager;
class BrEdrDiscoveryManager;
class PairingDelegate;
class LowEnergyAddressManager;
class LowEnergyAdvertisingManager;
class LowEnergyDiscoveryManager;

// TODO(BT-734): Consider removing this identifier from the bt-host layer.
class AdapterId : public Identifier<uint64_t> {
 public:
  constexpr explicit AdapterId(uint64_t value) : Identifier<uint64_t>(value) {}
  AdapterId() = default;
};

// Represents the host-subsystem state for a Bluetooth controller. All
// asynchronous callbacks are posted on the Loop on which this Adapter
// instance is created.
//
// This class is not thread-safe and it is intended to be created, deleted, and
// accessed on the same event loop. No internal locking is provided.
//
// NOTE: We currently only support primary controllers. AMP controllers are not
// supported.
class Adapter final {
 public:
  // There must be an async_t dispatcher registered as a default when an Adapter
  // instance is created. The Adapter instance will use it for all of its
  // asynchronous tasks.
  //
  // Optionally, a data domain may be passed for testing purposes as |data_domain|. If nullopt is
  // passed, then the Adapter will create and initialize its own data domain.
  explicit Adapter(fxl::WeakPtr<hci::Transport> hci, fxl::WeakPtr<gatt::GATT> gatt,
                   std::optional<fbl::RefPtr<data::Domain>> data_domain);
  ~Adapter();

  // Returns a uniquely identifier for this adapter on the current system.
  AdapterId identifier() const { return identifier_; }

  // Initializes the host-subsystem state for the HCI device this was created
  // for. This performs the initial HCI transport set up. Returns false if an
  // immediate error occurs. Otherwise this returns true and asynchronously
  // notifies the caller on the initialization status via |callback|.
  //
  // After successful initialization, |transport_closed_callback| will be
  // invoked when the underlying HCI transport closed for any reason (e.g. the
  // device disappeared or the transport channels were closed for an unknown
  // reason). The implementation is responsible for cleaning up this adapter by
  // calling ShutDown().
  using InitializeCallback = fit::function<void(bool success)>;
  bool Initialize(InitializeCallback callback, fit::closure transport_closed_callback);

  // Shuts down this Adapter. Invokes |callback| when shut down has completed.
  // TODO(armansito): This needs to do several things to potentially preserve
  // the state of various sub-protocols. For now we keep the interface pretty
  // simple.
  void ShutDown();

  // Returns true if the Initialize() sequence has started but not completed yet
  // (i.e. the InitializeCallback that was passed to Initialize() has not yet
  // been called).
  bool IsInitializing() const { return init_state_ == State::kInitializing; }

  // Returns true if this Adapter has been fully initialized.
  bool IsInitialized() const { return init_state_ == State::kInitialized; }

  // Returns the global adapter setting parameters.
  const AdapterState& state() const { return state_; }

  // Returns a weak pointer to this adapter.
  fxl::WeakPtr<Adapter> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // Returns this Adapter's peer cache.
  const PeerCache& peer_cache() const { return peer_cache_; }

  // Returns this Adapter's BR/EDR connection manager.
  BrEdrConnectionManager* bredr_connection_manager() const {
    return bredr_connection_manager_.get();
  }

  // Returns this Adapter's BR/EDR discovery manager.
  BrEdrDiscoveryManager* bredr_discovery_manager() const { return bredr_discovery_manager_.get(); }

  // Returns this Adapter's SDP server.
  sdp::Server* sdp_server() const { return sdp_server_.get(); }

  // Returns this Adapter's LE local address manager.
  LowEnergyAddressManager* le_address_manager() const {
    ZX_DEBUG_ASSERT(le_address_manager_);
    return le_address_manager_.get();
  }

  // Returns this Adapter's LE discovery manager.
  LowEnergyDiscoveryManager* le_discovery_manager() const {
    ZX_DEBUG_ASSERT(le_discovery_manager_);
    return le_discovery_manager_.get();
  }

  // Returns this Adapter's LE connection manager.
  LowEnergyConnectionManager* le_connection_manager() const {
    ZX_DEBUG_ASSERT(le_connection_manager_);
    return le_connection_manager_.get();
  }

  // Returns this Adapter's LE advertising manager.
  LowEnergyAdvertisingManager* le_advertising_manager() const {
    ZX_DEBUG_ASSERT(le_advertising_manager_);
    return le_advertising_manager_.get();
  }

  // Returns this Adapter's peer cache.
  PeerCache* peer_cache() { return &peer_cache_; }

  // Add a previously bonded device to the peer cache and set it up for
  // auto-connect procedures.
  bool AddBondedPeer(BondingData bonding_data);

  // Assigns a pairing delegate to this adapter. This PairingDelegate and its
  // I/O capabilities will be used for all future pairing procedures. Setting a
  // new PairingDelegate cancels all ongoing pairing procedures.
  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate);

  // Returns true if this adapter is currently in discoverable mode on the LE or BR/EDR transports.
  bool IsDiscoverable() const;

  // Returns true if any discovery process (LE or BR/EDR) is running on this
  // adapter.
  bool IsDiscovering() const;

  // Sets the Local Name of this adapter, for both BR/EDR discoverability and
  // public LE services.
  void SetLocalName(std::string name, hci::StatusCallback callback);

  // Sets the Device Class of this adapter.
  void SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback);

  // Returns a duplicate read-only version of the Inspect VMO.
  zx::vmo InspectVmo() const { return inspector_.DuplicateVmo(); }

  // Assign a callback to be notified when a connection is automatically
  // established to a bonded LE peer in the directed connectable mode (Vol 3,
  // Part C, 9.3.3).
  using AutoConnectCallback = fit::function<void(LowEnergyConnectionRefPtr)>;
  void set_auto_connect_callback(AutoConnectCallback callback) {
    auto_conn_cb_ = std::move(callback);
  }

 private:
  // Second step of the initialization sequence. Called by Initialize() when the
  // first batch of HCI commands have been sent.
  void InitializeStep2(InitializeCallback callback);

  // Third step of the initialization sequence. Called by InitializeStep2() when
  // the second batch of HCI commands have been sent.
  void InitializeStep3(InitializeCallback callback);

  // Fourth step of the initialization sequence. Called by InitializeStep3()
  // when the third batch of HCI commands have been sent.
  void InitializeStep4(InitializeCallback callback);

  // Assigns properties to |adapter_node_| using values discovered during other initialization
  // steps.
  void UpdateInspectProperties();

  // Builds and returns the HCI event mask based on our supported host side
  // features and controller capabilities. This is used to mask events that we
  // do not know how to handle.
  uint64_t BuildEventMask();

  // Builds and returns the LE event mask based on our supported host side
  // features and controller capabilities. This is used to mask LE events that
  // we do not know how to handle.
  uint64_t BuildLEEventMask();

  // Called by ShutDown() and during Initialize() in case of failure. This
  // synchronously cleans up the transports and resets initialization state.
  void CleanUp();

  // Called by Transport after it has been unexpectedly closed.
  void OnTransportClosed();

  // Called when a directed connectable advertisement is received from a bonded
  // LE device. This amounts to a connection request from a bonded peripheral
  // which is handled by routing the request to |le_connection_manager_| to
  // initiate a Direct Connection Establishment procedure (Vol 3, Part C,
  // 9.3.8).
  void OnLeAutoConnectRequest(Peer* peer);

  // Called by |le_address_manager_| to query whether it is currently allowed to
  // reconfigure the LE random address.
  bool IsLeRandomAddressChangeAllowed();

  // Must be initialized first so that child nodes can be passed to other constructors.
  inspect::Inspector inspector_;
  inspect::Node adapter_node_;
  struct InspectProperties {
    inspect::StringProperty adapter_id;
    inspect::StringProperty hci_version;
    inspect::UintProperty bredr_max_num_packets;
    inspect::UintProperty bredr_max_data_length;
    inspect::UintProperty le_max_num_packets;
    inspect::UintProperty le_max_data_length;
    inspect::StringProperty lmp_features;
    inspect::StringProperty le_features;
  };
  InspectProperties inspect_properties_;

  // Uniquely identifies this adapter on the current system.
  AdapterId identifier_;

  async_dispatcher_t* dispatcher_;
  fxl::WeakPtr<hci::Transport> hci_;

  // Callback invoked to notify clients when the underlying transport is closed.
  fit::closure transport_closed_cb_;

  // Parameters relevant to the initialization sequence.
  // TODO(armansito): The Initialize()/ShutDown() pattern has become common
  // enough in this project that it might be worth considering moving the
  // init-state-keeping into an abstract base.
  enum State {
    kNotInitialized = 0,
    kInitializing,
    kInitialized,
  };
  std::atomic<State> init_state_;
  std::unique_ptr<hci::SequentialCommandRunner> init_seq_runner_;

  // Contains the global adapter state.
  AdapterState state_;

  // The maximum LMP feature page that we will read.
  size_t max_lmp_feature_page_index_;

  // Provides access to discovered, connected, and/or bonded remote Bluetooth
  // devices.
  PeerCache peer_cache_;

  // The data domain used by GAP to interact with L2CAP and RFCOMM layers.
  fbl::RefPtr<data::Domain> data_domain_;

  // The GATT profile. We use this reference to add and remove data bearers and
  // for service discovery.
  fxl::WeakPtr<gatt::GATT> gatt_;

  // Objects that abstract the controller for connection and advertising
  // procedures.
  std::unique_ptr<hci::LowEnergyAdvertiser> hci_le_advertiser_;
  std::unique_ptr<hci::LowEnergyConnector> hci_le_connector_;
  std::unique_ptr<hci::LowEnergyScanner> hci_le_scanner_;

  // Objects that perform LE procedures.
  std::unique_ptr<LowEnergyAddressManager> le_address_manager_;
  std::unique_ptr<LowEnergyDiscoveryManager> le_discovery_manager_;
  std::unique_ptr<LowEnergyConnectionManager> le_connection_manager_;
  std::unique_ptr<LowEnergyAdvertisingManager> le_advertising_manager_;

  // Objects that perform BR/EDR procedures.
  std::unique_ptr<BrEdrConnectionManager> bredr_connection_manager_;
  std::unique_ptr<BrEdrDiscoveryManager> bredr_discovery_manager_;
  std::unique_ptr<sdp::Server> sdp_server_;

  // Callback to propagate ownership of an auto-connected LE link.
  AutoConnectCallback auto_conn_cb_;

  fxl::ThreadChecker thread_checker_;

  // This must remain the last member to make sure that all weak pointers are
  // invalidating before other members are destroyed.
  fxl::WeakPtrFactory<Adapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Adapter);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_H_
