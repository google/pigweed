// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_DEVICE_WRAPPER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_DEVICE_WRAPPER_H_

#include <zircon/status.h>
#include <zircon/types.h>

#include <ddk/protocol/bt/hci.h>
#include <lib/zx/channel.h>

#include "src/lib/files/unique_fd.h"
#include "lib/fxl/macros.h"

namespace btlib {
namespace hci {

// A DeviceWrapper abstracts over a Bluetooth HCI device object and its ioctls.
class DeviceWrapper {
 public:
  virtual ~DeviceWrapper() = default;

  // Returns the command/event channel handle for this device. Returns an
  // invalid handle on failure.
  virtual zx::channel GetCommandChannel() = 0;

  // Returns the ACL data channel handle for this device. Returns an invalid
  // handle on failure.
  virtual zx::channel GetACLDataChannel() = 0;
};

// A DeviceWrapper that obtains channels by invoking bt-hci ioctls on a devfs
// file descriptor.
class IoctlDeviceWrapper : public DeviceWrapper {
 public:
  // |device_fd| must be a valid file descriptor to a Bluetooth HCI device.
  explicit IoctlDeviceWrapper(fxl::UniqueFD device_fd);
  ~IoctlDeviceWrapper() override = default;

  // DeviceWrapper overrides. These methods directly return the handle obtained
  // via the corresponding ioctl.
  zx::channel GetCommandChannel() override;
  zx::channel GetACLDataChannel() override;

 private:
  fxl::UniqueFD device_fd_;

  FXL_DISALLOW_COPY_AND_ASSIGN(IoctlDeviceWrapper);
};

// A DeviceWrapper that obtains channels by calling bt-hci protocol ops.
class DdkDeviceWrapper : public DeviceWrapper {
 public:
  // The contents of |hci| must remain valid while this object is in use.
  explicit DdkDeviceWrapper(const bt_hci_protocol_t& hci);
  ~DdkDeviceWrapper() override = default;

  // DeviceWrapper overrides:
  zx::channel GetCommandChannel() override;
  zx::channel GetACLDataChannel() override;

 private:
  bt_hci_protocol_t hci_proto_;

  FXL_DISALLOW_COPY_AND_ASSIGN(DdkDeviceWrapper);
};

// A pass-through DeviceWrapper that returns the channel endpoints that it is
// initialized with. This is useful for test scenarios.
class DummyDeviceWrapper : public DeviceWrapper {
 public:
  // The constructor takes ownership of the provided channels and simply returns
  // them when asked for them.
  DummyDeviceWrapper(zx::channel cmd_channel, zx::channel acl_data_channel);
  ~DummyDeviceWrapper() override = default;

  // DeviceWrapper overrides. Since these methods simply forward the handles
  // they were initialized with, the internal handles will be moved and
  // invalidated after the first call to these methods. Subsequent calls will
  // always return an invalid handle.
  zx::channel GetCommandChannel() override;
  zx::channel GetACLDataChannel() override;

 private:
  zx::channel cmd_channel_;
  zx::channel acl_data_channel_;

  FXL_DISALLOW_COPY_AND_ASSIGN(DummyDeviceWrapper);
};

}  // namespace hci
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_DEVICE_WRAPPER_H_
