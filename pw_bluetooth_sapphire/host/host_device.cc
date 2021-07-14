// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "host_device.h"

#include <lib/inspect/cpp/inspect.h>
#include <zircon/status.h>

#include "host.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/device_wrapper.h"

namespace bthost {
namespace {

// bt-gatt-svc devices are published for HID-over-GATT only.
constexpr bt::UUID kHogUuid(uint16_t{0x1812});

const char* kDeviceName = "bt_host";

}  // namespace

HostDevice::HostDevice(zx_device_t* parent)
    : HostDeviceType(parent), loop_(&kAsyncLoopConfigNoAttachToCurrentThread) {
  ZX_DEBUG_ASSERT(parent);

  inspect_.GetRoot().CreateString("name", kDeviceName, &inspect_);
}

zx_status_t HostDevice::Bind() {
  bt_log(DEBUG, "bt-host", "bind");

  std::lock_guard<std::mutex> lock(mtx_);

  zx_status_t status = device_get_protocol(parent_, ZX_PROTOCOL_BT_HCI, &hci_proto_);
  if (status != ZX_OK) {
    bt_log(ERROR, "bt-host", "failed to obtain bt-hci protocol ops: %s",
           zx_status_get_string(status));
    return status;
  }

  if (!hci_proto_.ops) {
    bt_log(ERROR, "bt-host", "bt-hci device ops required!");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto_.ops->open_command_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_command_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto_.ops->open_acl_data_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_acl_data_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto_.ops->open_snoop_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_snoop_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  device_add_args_t args = {
      .version = DEVICE_ADD_ARGS_VERSION,
      .proto_id = ZX_PROTOCOL_BT_HOST,
      .flags = DEVICE_ADD_NON_BINDABLE,
      .inspect_vmo = inspect_.DuplicateVmo().release(),
  };
  status = DdkAdd("bt_host", args);

  if (status != ZX_OK) {
    bt_log(ERROR, "bt-host", "Failed to publish device: %s", zx_status_get_string(status));
    return status;
  }

  return status;
  // Since we define an init hook, Init will be called by the DDK after this method finishes.
}

void HostDevice::DdkInit(ddk::InitTxn txn) {
  bt_log(DEBUG, "bt-host", "init");

  std::lock_guard<std::mutex> lock(mtx_);

  auto vendor_result = GetVendorProtocol();
  if (vendor_result.is_ok()) {
    vendor_proto_ = vendor_result.value();
  } else {
    bt_log(WARN, "bt-host", "failed to obtain bt-vendor protocol ops: %s",
           zx_status_get_string(vendor_result.error()));
  }

  loop_.StartThread("bt-host (gap)");

  // Send the bootstrap message to Host. The Host object can only be accessed on
  // the Host thread.
  async::PostTask(loop_.dispatcher(), [this, txn{std::move(txn)}]() mutable {
    bt_log(TRACE, "bt-host", "host thread start");

    std::lock_guard<std::mutex> lock(mtx_);
    host_ = fxl::MakeRefCounted<Host>(hci_proto_, vendor_proto_);
    bt_host_node_ = inspect_.GetRoot().CreateChild("bt-host");
    host_->Initialize(bt_host_node_, [this, txn{std::move(txn)}](bool success) mutable {
      std::lock_guard<std::mutex> lock(mtx_);

      // host_ must be defined here as Bind() must have been called and the runloop has not
      // yet been been drained in Unbind().
      ZX_DEBUG_ASSERT(host_);

      if (!success) {
        bt_log(ERROR, "bt-host", "failed to initialize adapter; cleaning up");
        txn.Reply(ZX_ERR_INTERNAL);
        // DDK will call Unbind here to clean up.
      } else {
        bt_log(DEBUG, "bt-host", "adapter initialized; make device visible");
        host_->gatt()->RegisterRemoteServiceWatcher(
            fit::bind_member(this, &HostDevice::OnRemoteGattServiceAdded));
        txn.Reply(ZX_OK);
        return;
      }
    });
  });
}

void HostDevice::DdkUnbind(ddk::UnbindTxn txn) {
  bt_log(DEBUG, "bt-host", "unbind");

  {
    std::lock_guard<std::mutex> lock(mtx_);

    // Do this immediately to stop receiving new service callbacks.
    bt_log(TRACE, "bt-host", "removing GATT service watcher");
    ignore_gatt_services_ = true;

    async::PostTask(loop_.dispatcher(), [this] {
      std::lock_guard<std::mutex> lock(mtx_);
      host_->ShutDown();
      host_ = nullptr;
      loop_.Quit();
    });

    // Don't hold lock waiting on the loop to terminate.
  }

  // Make sure that the ShutDown task runs before this returns.
  bt_log(TRACE, "bt-host", "waiting for shut down tasks to complete");
  loop_.JoinThreads();

  txn.Reply();

  bt_log(DEBUG, "bt-host", "GAP has been shut down");
}

void HostDevice::DdkRelease() {
  bt_log(DEBUG, "bt-host", "release");
  delete this;
}

void HostDevice::Open(OpenRequestView request, OpenCompleter::Sync& completer) {
  std::lock_guard<std::mutex> lock(mtx_);

  // This is called from the fidl operation OpenChannelOp.  No fidl calls will be delivered to the
  // driver before host_ is initialized by Init(), and no fidl calls
  // will be delivered after the DDK calls Unbind() and host_ is removed.
  ZX_DEBUG_ASSERT(host_);

  // Tell Host to start processing messages on this handle.
  async::PostTask(loop_.dispatcher(), [host = host_, chan = std::move(request->channel)]() mutable {
    host->BindHostInterface(std::move(chan));
  });
}

void HostDevice::OnRemoteGattServiceAdded(bt::gatt::PeerId peer_id,
                                          fbl::RefPtr<bt::gatt::RemoteService> service) {
  TRACE_DURATION("bluetooth", "HostDevice::OnRemoteGattServiceAdded");

  // Only publish children for HID-over-GATT.
  if (service->uuid() != kHogUuid) {
    return;
  }

  std::lock_guard<std::mutex> lock(mtx_);

  if (ignore_gatt_services_) {
    return;
  }

  // This is run on the host event loop. Bind(), Init() and Unbind() should maintain the invariant
  // that  host_ are initialized when the event loop is running.
  ZX_DEBUG_ASSERT(host_);

  __UNUSED zx_status_t status = GattRemoteServiceDevice::Publish(zxdev(), peer_id, service);
}

fpromise::result<bt_vendor_protocol_t, zx_status_t> HostDevice::GetVendorProtocol() {
  bt_vendor_protocol_t vendor_proto = {};
  zx_status_t status = device_get_protocol(parent_, ZX_PROTOCOL_BT_VENDOR, &vendor_proto);
  if (status != ZX_OK) {
    return fpromise::error(status);
  }

  if (!vendor_proto.ops) {
    bt_log(WARN, "bt-host", "bt-vendor device ops required");
    return fpromise::error(ZX_ERR_NOT_SUPPORTED);
  }

  if (!vendor_proto.ops->get_features) {
    bt_log(WARN, "bt-host", "bt-vendor op required: get_features");
    return fpromise::error(ZX_ERR_NOT_SUPPORTED);
  }

  if (!vendor_proto.ops->encode_command) {
    bt_log(WARN, "bt-host", "bt-vendor op required: encode_command");
    return fpromise::error(ZX_ERR_NOT_SUPPORTED);
  }

  return fpromise::ok(vendor_proto);
}

}  // namespace bthost
