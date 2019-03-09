// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_

#include <mutex>

#include <lib/async-loop/cpp/loop.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include <ddk/device.h>
#include <ddk/driver.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt_remote_service_device.h"
#include "src/connectivity/bluetooth/core/bt-host/host.h"

#include "lib/fxl/macros.h"

namespace bthost {

class Host;

// Represents a bt-host device. This object relays device events to the host
// thread's event loop to be processed by the Host.
class HostDevice final {
 public:
  explicit HostDevice(zx_device_t* device);

  zx_status_t Bind();

 private:
  // Protocol trampolines.
  static void DdkUnbind(void* ctx) { static_cast<HostDevice*>(ctx)->Unbind(); }

  static void DdkRelease(void* ctx) {
    static_cast<HostDevice*>(ctx)->Release();
  }

  static zx_status_t DdkIoctl(void* ctx, uint32_t op, const void* in_buf,
                              size_t in_len, void* out_buf, size_t out_len,
                              size_t* out_actual) {
    return static_cast<HostDevice*>(ctx)->Ioctl(op, in_buf, in_len, out_buf,
                                                out_len, out_actual);
  }

  void Unbind();
  void Release();
  zx_status_t Ioctl(uint32_t op, const void* in_buf, size_t in_len,
                    void* out_buf, size_t out_len, size_t* out_actual);

  // Called when a new remote GATT service has been found.
  void OnRemoteGattServiceAdded(
      btlib::gatt::DeviceId peer_id,
      fbl::RefPtr<btlib::gatt::RemoteService> service);

  void CleanUp() __TA_REQUIRES(mtx_);

  zx_device_t* dev_;     // The bt-host device we published.
  zx_device_t* parent_;  // The parent bt-hci device.

  // The base DDK device ops.
  zx_protocol_device_t dev_proto_ = {};

  // Guards access to members below.
  std::mutex mtx_;

  // Map of child DDK gatt devices
  std::unordered_map<GattRemoteServiceDevice*,
                     std::unique_ptr<GattRemoteServiceDevice>>
      gatt_devices_;

  // Host processes all its messages on |loop_|. |loop_| is initialized to run
  // in its own thread.
  //
  // This is necessary as Host owns FIDL bindings which require a
  // single-threaded dispatcher.
  async::Loop loop_;
  fxl::RefPtr<Host> host_ __TA_GUARDED(mtx_);

  FXL_DISALLOW_COPY_AND_ASSIGN(HostDevice);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
