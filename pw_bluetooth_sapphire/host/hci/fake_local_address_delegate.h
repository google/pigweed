// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"

namespace bt::hci {

class FakeLocalAddressDelegate : public LocalAddressDelegate {
 public:
  FakeLocalAddressDelegate() = default;
  ~FakeLocalAddressDelegate() override = default;

  std::optional<UInt128> irk() const override { return std::nullopt; }
  DeviceAddress identity_address() const override { return identity_address_; }
  void EnsureLocalAddress(AddressCallback callback) override;

  // If set to true EnsureLocalAddress runs its callback asynchronously.
  void set_async(bool value) { async_ = value; }

  void set_identity_address(const DeviceAddress& value) { identity_address_ = value; }
  void set_local_address(const DeviceAddress& value) { local_address_ = value; }

 private:
  bool async_ = false;
  DeviceAddress local_address_ = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  DeviceAddress identity_address_ = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_
