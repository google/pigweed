// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_address_manager.h"

#include "gap.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::gap {

LowEnergyAddressManager::LowEnergyAddressManager(const DeviceAddress& public_address,
                                                 StateQueryDelegate delegate,
                                                 hci::CommandChannel::WeakPtr cmd_channel,
                                                 pw::async::Dispatcher& dispatcher)
    : dispatcher_(dispatcher),
      delegate_(std::move(delegate)),
      cmd_(std::move(cmd_channel)),
      privacy_enabled_(false),
      public_(public_address),
      needs_refresh_(false),
      refreshing_(false),
      weak_self_(this) {
  BT_DEBUG_ASSERT(public_.type() == DeviceAddress::Type::kLEPublic);
  BT_DEBUG_ASSERT(delegate_);
  BT_DEBUG_ASSERT(cmd_.is_alive());
}

LowEnergyAddressManager::~LowEnergyAddressManager() { CancelExpiry(); }

void LowEnergyAddressManager::EnablePrivacy(bool enabled) {
  if (enabled == privacy_enabled_) {
    bt_log(DEBUG, "gap-le", "privacy already %s", (enabled ? "enabled" : "disabled"));
    return;
  }

  privacy_enabled_ = enabled;

  if (!enabled) {
    CleanUpPrivacyState();
    ResolveAddressRequests();
    NotifyAddressUpdate();
    return;
  }

  needs_refresh_ = true;

  TryRefreshRandomAddress();
}

void LowEnergyAddressManager::EnsureLocalAddress(AddressCallback callback) {
  BT_DEBUG_ASSERT(callback);

  // Report the address right away if it doesn't need refreshing.
  if (!needs_refresh_) {
    callback(current_address());
    return;
  }

  address_callbacks_.push(std::move(callback));
  TryRefreshRandomAddress();
}

void LowEnergyAddressManager::TryRefreshRandomAddress() {
  if (!privacy_enabled_ || !needs_refresh_) {
    bt_log(DEBUG, "gap-le", "address does not need refresh");
    return;
  }

  if (refreshing_) {
    bt_log(DEBUG, "gap-le", "address update in progress");
    return;
  }

  if (!CanUpdateRandomAddress()) {
    bt_log(DEBUG, "gap-le", "deferring local address refresh due to ongoing procedures");
    // Don't stall procedures that requested the current address while in this
    // state.
    ResolveAddressRequests();
    return;
  }

  CancelExpiry();
  refreshing_ = true;

  DeviceAddress random_addr;
  if (irk_) {
    random_addr = sm::util::GenerateRpa(*irk_);
  } else {
    random_addr = sm::util::GenerateRandomAddress(/*is_static=*/false);
  }

  auto cmd = hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LESetRandomAddressCommandWriter>(
      hci_spec::kLESetRandomAddress);
  cmd.view_t().random_address().CopyFrom(random_addr.value().view());

  auto self = weak_self_.GetWeakPtr();
  auto cmd_complete_cb = [self, this, random_addr](auto id, const hci::EventPacket& event) {
    if (!self.is_alive()) {
      return;
    }

    refreshing_ = false;

    if (!privacy_enabled_) {
      bt_log(DEBUG, "gap-le", "ignore random address result while privacy is disabled");
      return;
    }

    if (!hci_is_error(event, TRACE, "gap-le", "failed to update random address")) {
      needs_refresh_ = false;
      random_ = random_addr;
      bt_log(INFO, "gap-le", "random address updated: %s", bt_str(*random_));

      // Set the new random address to expire in kPrivateAddressTimeout.
      random_address_expiry_task_.set_function(
          [this](pw::async::Context /*ctx*/, pw::Status status) {
            if (status.ok()) {
              needs_refresh_ = true;
              TryRefreshRandomAddress();
            }
          });
      random_address_expiry_task_.PostAfter(kPrivateAddressTimeout);

      // Notify any listeners of the change in device address.
      NotifyAddressUpdate();
    }

    ResolveAddressRequests();
  };

  cmd_->SendCommand(std::move(cmd), std::move(cmd_complete_cb));
}

void LowEnergyAddressManager::CleanUpPrivacyState() {
  privacy_enabled_ = false;
  needs_refresh_ = false;
  CancelExpiry();
}

void LowEnergyAddressManager::CancelExpiry() { random_address_expiry_task_.Cancel(); }

bool LowEnergyAddressManager::CanUpdateRandomAddress() const {
  BT_DEBUG_ASSERT(delegate_);
  return delegate_();
}

void LowEnergyAddressManager::ResolveAddressRequests() {
  auto address = current_address();
  auto q = std::move(address_callbacks_);
  bt_log(DEBUG, "gap-le", "using local address %s", address.ToString().c_str());
  while (!q.empty()) {
    q.front()(address);
    q.pop();
  }
}

void LowEnergyAddressManager::NotifyAddressUpdate() {
  auto address = current_address();
  for (auto& cb : address_changed_callbacks_) {
    cb(address);
  }
}

}  // namespace bt::gap
