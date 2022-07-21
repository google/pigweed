// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_

#include <fidl/fuchsia.hardware.bluetooth/cpp/wire.h>
#include <fuchsia/hardware/bt/vendor/c/banjo.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fitx/result.h>

#include <mutex>

#include <ddktl/device.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/host.h"

namespace bthost {

// Represents a bt-host device. This object relays device events to the host
// thread's event loop to be processed by the Host.
class HostDevice;
using HostDeviceType =
    ddk::Device<HostDevice, ddk::Initializable,
                ddk::Messageable<fuchsia_hardware_bluetooth::Host>::Mixin, ddk::Unbindable>;
class HostDevice final : public HostDeviceType {
 public:
  explicit HostDevice(zx_device_t* parent);
  zx_status_t Bind();

  // DDK methods
  void DdkInit(ddk::InitTxn txn);
  void DdkUnbind(ddk::UnbindTxn txn);
  void DdkRelease();

 private:
  // Open a new channel to the host. Send that channel to the fidl client
  // or respond with an error if the channel could not be opened.
  // Returns the status of the fidl send operation.
  void Open(OpenRequestView request, OpenCompleter::Sync& completer);

  // Get the vendor protocol that is supported by the underlying host device.
  fitx::result<zx_status_t, bt_vendor_protocol_t> GetVendorProtocol();

  // Initializes the host and the host thread.
  // Calls |cb| when complete with a success or error.
  void InitializeHostLocked(fit::function<void(bool success)> callback) __TA_REQUIRES(mtx_);

  // Shuts down the host thread, destroying and shutting down the host.
  void ShutdownHost();

  // Guards access to members below.
  std::mutex mtx_;

  // HCI protocol struct
  bt_hci_protocol_t hci_proto_;

  // BtVendor protocol struct.
  std::optional<bt_vendor_protocol_t> vendor_proto_;

  // Inspector for driver inspect tree. This object is thread-safe.
  inspect::Inspector inspect_;

  // Root inspect node for bt_host
  inspect::Node bt_host_node_;

  // Host processes all its messages on |loop_|. |loop_| is initialized to run
  // in its own thread.
  //
  // This is necessary as Host owns FIDL bindings which require a
  // single-threaded dispatcher.
  async::Loop loop_;
  fbl::RefPtr<Host> host_ __TA_GUARDED(mtx_);

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostDevice);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
