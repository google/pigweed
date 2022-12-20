// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADDRESS_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADDRESS_MANAGER_H_

#include <lib/async/cpp/task.h>

#include <optional>
#include <queue>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace gap {

// Manages the local LE device address used in scan, legacy advertising, and
// connection initiation procedures. The primary purpose of this class is to
// defer updating the random device address if we believe that doing so is
// disallowed by the controller. This is the case when scanning or legacy
// advertising is enabled, according to the Core Spec v5.3, Vol 4, Part E,
// 7.8.4.
//
// Procedures that need to know the value of the local address (both connection
// and advertising procedures need to assign this to any resultant
// hci::Connection object for SMP pairing to function correctly) should call the
// EnsureLocalAddress() method to obtain it and to lazily refresh the address
// if required.
//
// The type and value of the local address depends on whether or not the privacy
// feature is in use. The potential states are as follows:
//
//   * When privacy is DISABLED, the local address type and its value match
//     the public device address that this object gets initialized with.
//
//   * When privacy is ENABLED, the exact type and value depends on the state of
//     link layer procedures at that time. The "HCI LE Set Random Address"
//     command is used to assign the controller a random address, which it will
//     use for the next active scan, legacy advertising, or initiation command
//     with a random address type. A new local random address will be generated
//     at a regular interval (see kPrivateAddressTimeout in gap.h).
//
//     According to Vol 2, Part E, 7.8.4 the "HCI LE Set Random Address"
//     command is disallowed when scanning or legacy advertising are enabled.
//     Before any one of these procedures gets started, the EnsureLocalAddress()
//     method should be called to update the random address if it is allowed by
//     the controller (and the address needs a refresh). This function
//     asynchronously returns the device address that should be used by the
//     procedure.
//
// The state requested by EnablePrivacy() (enabled or disabled) may not take
// effect immediately if a scan, advertising, or connection procedure is in
// progress. The requested address type (public or private) will apply
// eventually when the controller allows it.
class LowEnergyAddressManager final : public hci::LocalAddressDelegate {
 public:
  // Function called when privacy is in use to determine if it is allowed to
  // assign a new random address to the controller. This must return false if
  // if scan, legacy advertising, and/or initiation procedures are in progress.
  using StateQueryDelegate = fit::function<bool()>;

  LowEnergyAddressManager(const DeviceAddress& public_address, StateQueryDelegate delegate,
                          hci::Transport::WeakPtr hci);
  ~LowEnergyAddressManager();

  // Assigns the IRK to generate a RPA for the next address refresh when privacy
  // is enabled.
  void set_irk(const std::optional<UInt128>& irk) { irk_ = irk; }

  // Enable or disable the privacy feature. When enabled, the controller will be
  // configured to use a new random address if it is currently allowed to do so.
  // If setting the random address is not allowed the update will be deferred
  // until the the next successful attempt triggered by a call to
  // TryRefreshRandomAddress().
  //
  // If an IRK has been assigned and |enabled| is true, then the generated
  // random addresses will each be a Resolvable Private Address that can be
  // resolved with the IRK. Otherwise, Non-resolvable Private Addresses will be
  // used.
  void EnablePrivacy(bool enabled);

  // Returns true if the privacy feature is currently enabled.
  bool PrivacyEnabled() const { return privacy_enabled_; }

  // LocalAddressDelegate overrides:
  std::optional<UInt128> irk() const override { return irk_; }
  DeviceAddress identity_address() const override { return public_; }
  void EnsureLocalAddress(AddressCallback callback) override;

  // Assign a callback to be notified any time the LE address changes.
  void register_address_changed_callback(AddressCallback callback) {
    address_changed_callbacks_.emplace_back(std::move(callback));
  };

  // Return the current address.
  const DeviceAddress& current_address() const {
    return (privacy_enabled_ && random_) ? *random_ : public_;
  }

 private:
  // Attempt to reconfigure the current random device address.
  void TryRefreshRandomAddress();

  // Clears all privacy related state such that the random address will not be
  // refreshed until privacy is re-enabled. |random_| is not modified and
  // continues to reflect the most recently configured random address.
  void CleanUpPrivacyState();

  void CancelExpiry();
  bool CanUpdateRandomAddress() const;
  void ResolveAddressRequests();

  // Notifies all address changed listeners of the change in device address.
  void NotifyAddressUpdate();

  StateQueryDelegate delegate_;
  hci::Transport::WeakPtr hci_;
  bool privacy_enabled_;

  // The public device address (i.e. BD_ADDR) that is assigned to the
  // controller.
  const DeviceAddress public_;

  // The random device address assigned to the controller by the most recent
  // successful HCI_LE_Set_Random_Address command. std::nullopt if the command
  // was never run successfully.
  std::optional<DeviceAddress> random_;

  // True if the random address needs a refresh. This is the case when
  //   a. Privacy is enabled and the random device address needs to get rotated;
  //      or
  //   b. Privacy has recently been enabled and the controller hasn't been
  //      programmed with the new address yet
  bool needs_refresh_;

  // True if a HCI command to update the random address is in progress.
  bool refreshing_;

  // The local identity resolving key. If present, it is used to generate RPAs
  // when privacy is enabled.
  std::optional<UInt128> irk_;

  // Callbacks waiting to be notified of the local address.
  std::queue<AddressCallback> address_callbacks_;
  // Callbacks waiting to be notified of a change in the local address.
  std::vector<AddressCallback> address_changed_callbacks_;

  // The task that executes when a random address expires and needs to be
  // refreshed.
  async::Task random_address_expiry_task_;

  fxl::WeakPtrFactory<LowEnergyAddressManager> weak_ptr_factory_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LowEnergyAddressManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_ADDRESS_MANAGER_H_
