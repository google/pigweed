// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_DEVICE_H_

#include <fuchsia/hardware/bluetooth/c/fidl.h>
#include <fuchsia/hardware/bt/vendor/c/banjo.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include <mutex>

#include <ddktl/device.h>
#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt_remote_service_device.h"
#include "src/connectivity/bluetooth/core/bt-host/host.h"

namespace bthost {

// Represents a bt-host device. This object relays device events to the host
// thread's event loop to be processed by the Host.
class HostDevice;
using HostDeviceType =
    ddk::Device<HostDevice, ddk::Initializable, ddk::Messageable, ddk::Unbindable>;
class HostDevice final : public HostDeviceType {
 public:
  explicit HostDevice(zx_device_t* parent);
  zx_status_t Bind();

  // DDK methods
  void DdkInit(ddk::InitTxn txn);
  zx_status_t DdkMessage(fidl_incoming_msg_t* msg, fidl_txn_t* txn);
  void DdkUnbind(ddk::UnbindTxn txn);
  void DdkRelease();

 private:
  // Open a new channel to the host. Send that channel to the fidl client
  // or respond with an error if the channel could not be opened.
  // Returns the status of the fidl send operation.
  zx_status_t OpenHostChannel(zx::channel channel);

  // Called when a new remote GATT service has been found.
  void OnRemoteGattServiceAdded(bt::gatt::PeerId peer_id,
                                fbl::RefPtr<bt::gatt::RemoteService> service);

  fit::result<bt_vendor_protocol_t, zx_status_t> GetVendorProtocol();

  // Guards access to members below.
  std::mutex mtx_;

  // Used to ignore new gatt services during & after unbinding.
  bool ignore_gatt_services_ __TA_GUARDED(mtx_) = false;

  // HCI protocol struct
  bt_hci_protocol_t hci_proto_;

  // BtVendor protocol struct.
  std::optional<bt_vendor_protocol_t> vendor_proto_;

  // Inspector for driver inspect tree. This object is thread-safe.
  inspect::Inspector inspect_;

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
