// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "host_device.h"

#include <lib/async-loop/default.h>
#include <zircon/status.h>

#include "host.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bthost {
namespace {

const char* kDeviceName = "bt_host";

}  // namespace

HostDevice::HostDevice(zx_device_t* parent)
    : HostDeviceType(parent),
      loop_(&kAsyncLoopConfigNoAttachToCurrentThread),
      pw_dispatcher_(loop_.dispatcher()),
      heap_dispatcher_(pw_dispatcher_) {
  BT_DEBUG_ASSERT(parent);

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
#ifndef NINSPECT
      .inspect_vmo = inspect_.DuplicateVmo().release(),
#endif  // NINSPECT
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
  if (vendor_result.is_error()) {
    bt_log(WARN, "bt-host", "failed to obtain bt-vendor protocol ops: %s",
           zx_status_get_string(vendor_result.error_value()));
  } else {
    vendor_proto_ = vendor_result.value();
  }

  InitializeHostLocked([this, txn{std::move(txn)}](bool success) mutable {
    std::lock_guard<std::mutex> lock(mtx_);
    // host_ must be defined here as Bind() must have been called and the runloop has not
    // yet been been drained in Unbind().
    BT_DEBUG_ASSERT(host_);

    if (!success) {
      bt_log(ERROR, "bt-host", "failed to initialize adapter; cleaning up");
      txn.Reply(ZX_ERR_INTERNAL);
      // DDK will call Unbind here to clean up.
    } else {
      bt_log(DEBUG, "bt-host", "adapter initialized; make device visible");
      txn.Reply(ZX_OK);
    }
  });
}

void HostDevice::InitializeHostLocked(fit::function<void(bool success)> callback) {
  loop_.StartThread("bt-host (gap)");

  // Send the bootstrap message to Host. The Host object can only be accessed on
  // the Host thread.
  heap_dispatcher_.Post(
      [this, callback{std::move(callback)}](pw::async::Context /*ctx*/, pw::Status status) mutable {
        if (!status.ok()) {
          bt_log(INFO, "bt-host", "dispatcher shut down before host initialization");
          return;
        }
        bt_log(TRACE, "bt-host", "host thread start");

        std::lock_guard<std::mutex> lock(mtx_);
        host_ = Host::Create(hci_proto_, vendor_proto_);
        bt_host_node_ = inspect_.GetRoot().CreateChild("bt-host");
        host_->Initialize(bt_host_node_, std::move(callback), [this]() {
          bt_log(WARN, "bt-host", "transport error, shutting down and removing host..");
          // TODO(fxbug.dev/52588): Consider destroying the host stack synchronously here, instead
          // of waiting for device unbinding.
          DdkAsyncRemove();
        });
      });
}

void HostDevice::ShutdownHost() {
  std::lock_guard<std::mutex> lock(mtx_);
  heap_dispatcher_.Post([this](pw::async::Context /*ctx*/, pw::Status status) {
    if (!status.ok()) {
      return;
    }
    std::lock_guard<std::mutex> lock(mtx_);
    host_->ShutDown();
    host_ = nullptr;
    loop_.Quit();
  });
}

void HostDevice::DdkUnbind(ddk::UnbindTxn txn) {
  bt_log(DEBUG, "bt-host", "unbind");

  ShutdownHost();

  // Make sure that the Shutdown task runs before this returns.
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
  BT_DEBUG_ASSERT(host_);

  // Tell Host to start processing messages on this handle.
  heap_dispatcher_.Post([host = host_, chan = std::move(request->channel)](
                            pw::async::Context /*ctx*/, pw::Status status) mutable {
    if (status.ok()) {
      host->BindHostInterface(chan.TakeChannel());
    }
  });
}

fit::result<zx_status_t, bt_vendor_protocol_t> HostDevice::GetVendorProtocol() {
  bt_vendor_protocol_t vendor_proto = {};
  zx_status_t status = device_get_protocol(parent_, ZX_PROTOCOL_BT_VENDOR, &vendor_proto);
  if (status != ZX_OK) {
    return fit::error(status);
  }

  if (!vendor_proto.ops) {
    bt_log(WARN, "bt-host", "bt-vendor device ops required");
    return fit::error(ZX_ERR_NOT_SUPPORTED);
  }

  if (!vendor_proto.ops->get_features) {
    bt_log(WARN, "bt-host", "bt-vendor op required: get_features");
    return fit::error(ZX_ERR_NOT_SUPPORTED);
  }

  if (!vendor_proto.ops->encode_command) {
    bt_log(WARN, "bt-host", "bt-vendor op required: encode_command");
    return fit::error(ZX_ERR_NOT_SUPPORTED);
  }

  return fit::ok(vendor_proto);
}

}  // namespace bthost
