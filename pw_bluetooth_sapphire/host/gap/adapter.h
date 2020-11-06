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
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter_state.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bonding_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
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

// TODO(fxbug.dev/1327): Consider removing this identifier from the bt-host layer.
class AdapterId : public Identifier<uint64_t> {
 public:
  constexpr explicit AdapterId(uint64_t value) : Identifier<uint64_t>(value) {}
  AdapterId() = default;
};

// Represents the host-subsystem state for a Bluetooth controller.
//
// This class is not guaranteed to be thread-safe and it is intended to be created, deleted, and
// accessed on the same event loop. No internal locking is provided.
//
// NOTE: We currently only support primary controllers. AMP controllers are not
// supported.
class Adapter {
 public:
  // Optionally, a FakeL2cap  may be passed for testing purposes as |l2cap|. If nullopt is
  // passed, then the Adapter will create and initialize its own L2cap.
  static std::unique_ptr<Adapter> Create(fxl::WeakPtr<hci::Transport> hci,
                                         fxl::WeakPtr<gatt::GATT> gatt,
                                         std::optional<fbl::RefPtr<l2cap::L2cap>> l2cap);
  virtual ~Adapter() = default;

  // Returns a uniquely identifier for this adapter on the current system.
  virtual AdapterId identifier() const = 0;

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
  virtual bool Initialize(InitializeCallback callback, fit::closure transport_closed_callback) = 0;

  // Shuts down this Adapter. Invokes |callback| when shut down has completed.
  // TODO(armansito): This needs to do several things to potentially preserve
  // the state of various sub-protocols. For now we keep the interface pretty
  // simple.
  virtual void ShutDown() = 0;

  // Returns true if the Initialize() sequence has started but not completed yet
  // (i.e. the InitializeCallback that was passed to Initialize() has not yet
  // been called).
  virtual bool IsInitializing() const = 0;

  // Returns true if this Adapter has been fully initialized.
  virtual bool IsInitialized() const = 0;

  // Returns the global adapter setting parameters.
  virtual const AdapterState& state() const = 0;

  // Returns a weak pointer to this adapter.
  virtual fxl::WeakPtr<Adapter> AsWeakPtr() = 0;

  // Returns this Adapter's BR/EDR connection manager.
  virtual BrEdrConnectionManager* bredr_connection_manager() const = 0;

  // Returns this Adapter's BR/EDR discovery manager.
  virtual BrEdrDiscoveryManager* bredr_discovery_manager() const = 0;

  // Returns this Adapter's SDP server.
  virtual sdp::Server* sdp_server() const = 0;

  // Returns this Adapter's LE local address manager.
  virtual LowEnergyAddressManager* le_address_manager() const = 0;

  // Returns this Adapter's LE discovery manager.
  virtual LowEnergyDiscoveryManager* le_discovery_manager() const = 0;

  // Returns this Adapter's LE connection manager.
  virtual LowEnergyConnectionManager* le_connection_manager() const = 0;

  // Returns this Adapter's LE advertising manager.
  virtual LowEnergyAdvertisingManager* le_advertising_manager() const = 0;

  // Returns this Adapter's peer cache.
  virtual PeerCache* peer_cache() = 0;

  // Add a previously bonded device to the peer cache and set it up for
  // auto-connect procedures.
  virtual bool AddBondedPeer(BondingData bonding_data) = 0;

  // Assigns a pairing delegate to this adapter. This PairingDelegate and its
  // I/O capabilities will be used for all future pairing procedures. Setting a
  // new PairingDelegate cancels all ongoing pairing procedures.
  virtual void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) = 0;

  // Returns true if this adapter is currently in discoverable mode on the LE or BR/EDR transports.
  virtual bool IsDiscoverable() const = 0;

  // Returns true if any discovery process (LE or BR/EDR) is running on this
  // adapter.
  virtual bool IsDiscovering() const = 0;

  // Sets the Local Name of this adapter, for both BR/EDR discoverability and
  // public LE services.
  virtual void SetLocalName(std::string name, hci::StatusCallback callback) = 0;

  // Sets the Device Class of this adapter.
  virtual void SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) = 0;

  // Assign a callback to be notified when a connection is automatically
  // established to a bonded LE peer in the directed connectable mode (Vol 3,
  // Part C, 9.3.3).
  using AutoConnectCallback = fit::function<void(LowEnergyConnectionRefPtr)>;
  virtual void set_auto_connect_callback(AutoConnectCallback callback) = 0;

  // Attach Adapter's inspect node as a child node under |parent| with the given |name|.
  virtual void AttachInspect(inspect::Node& parent, std::string name) = 0;
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_H_
