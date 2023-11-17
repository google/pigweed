// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_

#include "pw_async/heap_dispatcher.h"
#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/hci/local_address_delegate.h"

namespace bt::hci {

class FakeLocalAddressDelegate : public LocalAddressDelegate {
 public:
  explicit FakeLocalAddressDelegate(pw::async::Dispatcher& pw_dispatcher)
      : heap_dispatcher_(pw_dispatcher) {}
  ~FakeLocalAddressDelegate() override = default;

  std::optional<UInt128> irk() const override { return std::nullopt; }
  DeviceAddress identity_address() const override { return identity_address_; }
  void EnsureLocalAddress(AddressCallback callback) override;

  // If set to true EnsureLocalAddress runs its callback asynchronously.
  void set_async(bool value) { async_ = value; }

  void set_identity_address(const DeviceAddress& value) {
    identity_address_ = value;
  }
  void set_local_address(const DeviceAddress& value) { local_address_ = value; }

 private:
  bool async_ = false;
  DeviceAddress local_address_ =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  DeviceAddress identity_address_ =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {0});
  pw::async::HeapDispatcher heap_dispatcher_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOCAL_ADDRESS_DELEGATE_H_
