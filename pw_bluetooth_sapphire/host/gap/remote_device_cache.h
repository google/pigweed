// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_REMOTE_DEVICE_CACHE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_REMOTE_DEVICE_CACHE_H_

#include <unordered_map>

#include <fbl/function.h>
#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/identity_resolving_list.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/macros.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {

namespace common {
class ByteBuffer;
}  // namespace common

namespace hci {
struct LowEnergyScanResult;
}  // namespace hci

namespace gap {

// A RemoteDeviceCache provides access to remote Bluetooth devices that are
// known to the system.
class RemoteDeviceCache final {
 public:
  using DeviceCallback = fit::function<void(const RemoteDevice& device)>;
  using DeviceIdCallback = fit::function<void(DeviceId identifier)>;

  RemoteDeviceCache() = default;

  // Creates a new device entry using the given parameters, and returns a
  // (non-owning) pointer to that device. The caller must not retain the pointer
  // beyond the current dispatcher task, as the underlying RemoteDevice is owned
  // by |this| RemoteDeviceCache, and may be invalidated spontaneously.
  //
  // Returns nullptr if an entry matching |address| already exists in the cache,
  // including as a public identity of a device with a different technology.
  RemoteDevice* NewDevice(const common::DeviceAddress& address,
                          bool connectable);

  // Iterates over all current devices in the map, running |f| on each entry
  // synchronously. This is intended for IPC methods that request a list of
  // devices.
  //
  // Clients should use the FindDeviceBy*() methods below to interact with
  // RemoteDevice objects.
  void ForEach(DeviceCallback f);

  // Creates a new non-temporary device entry using the given |identifier| and
  // identity |address|. This is intended to initialize this RemoteDeviceCache
  // with previously bonded devices while bootstrapping a bt-host device. The
  // "device bonded" callback will not be invoked.
  //
  // This method is not intended for updating the bonding data of a device that
  // already exists the cache and returns false if a mapping for |identifier| or
  // |address| is already present. Use Store*Bond() methods to update pairing
  // information of an existing device.
  //
  // If a device already exists that has the same public identity address with a
  // different technology, this method will return false. The existing device
  // should be instead updated with new bond information to create a dual-mode
  // device.
  //
  bool AddBondedDevice(DeviceId identifier,
                       const common::DeviceAddress& address,
                       const sm::PairingData& bond_data,
                       const std::optional<sm::LTK>& link_key);

  // Update the device with the given identifier with new LE bonding
  // information. The device will be considered "bonded" and the bonded callback
  // will be notified. If the device is already bonded then bonding data will be
  // updated.
  //
  // If |bond_data| contains an |identity_address|, the device cache will be
  // updated with a new mapping from that address to this device identifier. If
  // the identity address already maps to an existing device, this method will
  // return false. TODO(armansito): Merge the devices instead of failing? What
  // happens if we obtain a LE identity address from a dual-mode device that
  // matches the BD_ADDR previously obtained from it over BR/EDR?
  bool StoreLowEnergyBond(DeviceId identifier,
                          const sm::PairingData& bond_data);

  // Update a device identified by BD_ADDR |address| with a new BR/EDR link key.
  // The device will be considered "bonded" and the bonded callback notified. If
  // the device is already bonded then the link key will be updated. Returns
  // false if the address does not match that of a known device.
  bool StoreBrEdrBond(const common::DeviceAddress& address,
                      const sm::LTK& link_key);

  // Returns the remote device with identifier |identifier|. Returns nullptr if
  // |identifier| is not recognized.
  RemoteDevice* FindDeviceById(DeviceId identifier) const;

  // Finds and returns a RemoteDevice with address |address| if it exists,
  // returns nullptr otherwise. Tries to resolve |address| if it is resolvable.
  // If |address| is of type kBREDR or kLEPublic, then this searches for devices
  // that have either type of address.
  RemoteDevice* FindDeviceByAddress(const common::DeviceAddress& address) const;

  // When set, |callback| will be invoked whenever a device is added or updated.
  void set_device_updated_callback(DeviceCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    device_updated_callback_ = std::move(callback);
  }

  // When set, |callback| will be invoked whenever a device is removed.
  void set_device_removed_callback(DeviceIdCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    device_removed_callback_ = std::move(callback);
  }

  // When this callback is set, |callback| will be invoked whenever the bonding
  // data of a device is updated and should be persisted. The caller must ensure
  // that |callback| outlives |this|.
  void set_device_bonded_callback(DeviceCallback callback) {
    ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
    device_bonded_callback_ = std::move(callback);
  }

  // Returns the number of devices that are currently in the device cache.
  size_t count() const { return devices_.size(); }

 private:
  class RemoteDeviceRecord final {
   public:
    RemoteDeviceRecord(std::unique_ptr<RemoteDevice> device,
                       fbl::Closure remove_device_callback)
        : device_(std::move(device)),
          removal_task_(std::move(remove_device_callback)) {}

    // The copy and move ctors cannot be implicitly defined, since
    // async::TaskClosure does not support those operations. Nor is any
    // meaningful explicit definition possible.
    RemoteDeviceRecord(const RemoteDeviceRecord&) = delete;
    RemoteDeviceRecord(RemoteDeviceRecord&&) = delete;

    RemoteDevice* device() const { return device_.get(); }

    // Returns a pointer to removal_task_, which can be used to (re-)schedule or
    // cancel |remove_device_callback|.
    async::TaskClosure* removal_task() { return &removal_task_; }

   private:
    std::unique_ptr<RemoteDevice> device_;
    async::TaskClosure removal_task_;
  };

  // Create and track a record of a remote device with a given |identifier|,
  // |address|, and connectability (|connectable|). Returns a pointer to the
  // inserted device or nullptr if |identifier| or |address| already exists in
  // the cache.
  RemoteDevice* InsertDeviceRecord(DeviceId identifier,
                                   const common::DeviceAddress& address,
                                   bool connectable);

  // Notifies interested parties that |device| has bonded
  // |device| must already exist in the cache.
  void NotifyDeviceBonded(const RemoteDevice& device);

  // Notifies interested parties that |device| has seen a significant change.
  // |device| must already exist in the cache.
  void NotifyDeviceUpdated(const RemoteDevice& device);

  // Updates the expiration time for |device|, if a temporary. Cancels expiry,
  // if a non-temporary. Pre-conditions:
  // - |device| must already exist in the cache
  // - can only be called from the thread that created |device|
  void UpdateExpiry(const RemoteDevice& device);

  // Updates the cache when an existing device is found to be dual-mode. Also
  // notifies listeners of the "device updated" callback.
  // |device| must already exist in the cache.
  void MakeDualMode(const RemoteDevice& device);

  // Removes |device| from this cache, and notifies listeners of the
  // removal.
  void RemoveDevice(RemoteDevice* device);

  // Search for an unique device ID by its device address |address|, by both
  // technologies if it is a public address. |address| should be already
  // resolved, if it is resolvable. If found, returns a valid device ID;
  // otherwise returns std::nullopt.
  std::optional<DeviceId> FindIdByAddress(
      const common::DeviceAddress& address) const;

  // Mapping from unique device IDs to RemoteDeviceRecords.
  // Owns the corresponding RemoteDevices.
  std::unordered_map<DeviceId, RemoteDeviceRecord> devices_;

  // Mapping from device addresses to unique device identifiers for all known
  // devices. This is used to look-up and update existing cached data for a
  // particular scan result so as to avoid creating duplicate entries for the
  // same device.
  //
  // Dual-mode devices shall have identity addresses of both technologies
  // mapped to the same ID, if the addresses have the same value.
  std::unordered_map<common::DeviceAddress, DeviceId> address_map_;

  // The LE identity resolving list used to resolve RPAs.
  IdentityResolvingList le_resolving_list_;

  DeviceCallback device_updated_callback_;
  DeviceIdCallback device_removed_callback_;
  DeviceCallback device_bonded_callback_;

  fxl::ThreadChecker thread_checker_;

  FXL_DISALLOW_COPY_AND_ASSIGN(RemoteDeviceCache);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_REMOTE_DEVICE_CACHE_H_
