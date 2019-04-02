// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOCAL_ADDRESS_DELEGATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOCAL_ADDRESS_DELEGATE_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"

namespace bt {
namespace hci {

class LocalAddressDelegate {
 public:
  virtual ~LocalAddressDelegate() = default;

  // Asynchronously returns the local LE controller address used by all LE link
  // layer procedures with the exception of 5.0 advertising sets. These include:
  //   - Legacy and extended scan requests;
  //   - Legacy and extended connection initiation;
  //   - Legacy advertising.
  //
  // There are two kinds of address that can be returned by this function:
  //   - A public device address (BD_ADDR) shared with the BR/EDR transport and
  //     typically factory-assigned.
  //   - A random device address that has been assigned to the controller by the
  //     host using the HCI_LE_Set_Random_Address command.
  //
  // This method runs |callback| when the procedure ends. |callback| may run
  // synchronously or asynchronously.
  using AddressCallback = fit::function<void(const common::DeviceAddress&)>;
  virtual void EnsureLocalAddress(AddressCallback callback) = 0;
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOCAL_ADDRESS_DELEGATE_H_
