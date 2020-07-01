// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_

#include <fuchsia/hardware/bluetooth/c/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include <mutex>

#include <ddk/device.h>
#include <ddk/driver.h>
#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt_remote_service_device.h"
#include "src/connectivity/bluetooth/core/bt-host/host.h"

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

  static void DdkRelease(void* ctx) { static_cast<HostDevice*>(ctx)->Release(); }

  // Route ddk fidl messages to the dispatcher function
  static zx_status_t DdkMessage(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    // Struct containing function pointers for all fidl ops to be dispatched on
    static constexpr fuchsia_hardware_bluetooth_Host_ops_t fidl_ops = {
        .Open = OpenFidlOp,
    };

    bt_log(DEBUG, "bt-host", "fidl message");
    return fuchsia_hardware_bluetooth_Host_dispatch(ctx, txn, msg, &fidl_ops);
  }

  // Fidl trampolines.
  static zx_status_t OpenFidlOp(void* ctx, zx_handle_t channel) {
    return static_cast<HostDevice*>(ctx)->OpenHostChannel(zx::channel(channel));
  }

  void Unbind();
  void Release();

  // Open a new channel to the host. Send that channel to the fidl client
  // or respond with an error if the channel could not be opened.
  // Returns the status of the fidl send operation.
  zx_status_t OpenHostChannel(zx::channel channel);

  // Called when a new remote GATT service has been found.
  void OnRemoteGattServiceAdded(bt::gatt::PeerId peer_id,
                                fbl::RefPtr<bt::gatt::RemoteService> service);

  void CleanUp() __TA_REQUIRES(mtx_);

  zx_device_t* dev_;     // The bt-host device we published.
  zx_device_t* parent_;  // The parent bt-hci device.

  // The base DDK device ops.
  zx_protocol_device_t dev_proto_ = {};

  // Guards access to members below.
  std::mutex mtx_;

  // Host processes all its messages on |loop_|. |loop_| is initialized to run
  // in its own thread.
  //
  // This is necessary as Host owns FIDL bindings which require a
  // single-threaded dispatcher.
  async::Loop loop_;
  fxl::RefPtr<Host> host_ __TA_GUARDED(mtx_);

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostDevice);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
