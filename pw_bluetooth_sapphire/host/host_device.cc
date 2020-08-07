// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "host_device.h"

#include <lib/inspect/cpp/inspect.h>
#include <zircon/status.h>

#include "host.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/device_wrapper.h"

namespace bthost {
namespace {

// bt-gatt-svc devices are published for HID-over-GATT only.
constexpr bt::UUID kHogUuid(uint16_t(0x1812));

const char* kDeviceName = "bt_host";

}  // namespace

HostDevice::HostDevice(zx_device_t* device)
    : dev_(nullptr), parent_(device), loop_(&kAsyncLoopConfigNoAttachToCurrentThread) {
  ZX_DEBUG_ASSERT(parent_);

  dev_proto_.version = DEVICE_OPS_VERSION;
  dev_proto_.init = &HostDevice::DdkInit;
  dev_proto_.unbind = &HostDevice::DdkUnbind;
  dev_proto_.release = &HostDevice::DdkRelease;
  dev_proto_.message = &HostDevice::DdkMessage;

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
      .name = kDeviceName,
      .ctx = this,
      .ops = &dev_proto_,
      .proto_id = ZX_PROTOCOL_BT_HOST,
      .flags = DEVICE_ADD_NON_BINDABLE,
      .inspect_vmo = inspect_.DuplicateVmo().release(),
  };

  status = device_add(parent_, &args, &dev_);
  if (status != ZX_OK) {
    bt_log(ERROR, "bt-host", "Failed to publish device: %s", zx_status_get_string(status));
    return status;
  }

  return status;
  // Since we define an init hook, Init will be called by the DDK after this method finishes.
}

void HostDevice::Init() {
  bt_log(DEBUG, "bt-host", "init");

  std::lock_guard<std::mutex> lock(mtx_);

  loop_.StartThread("bt-host (gap)");

  // Send the bootstrap message to Host. The Host object can only be accessed on
  // the Host thread.
  async::PostTask(loop_.dispatcher(), [this] {
    bt_log(TRACE, "bt-host", "host thread start");

    std::lock_guard<std::mutex> lock(mtx_);
    host_ = fxl::MakeRefCounted<Host>(hci_proto_);
    host_->Initialize(inspect_.GetRoot(), [this](bool success) {
      std::lock_guard<std::mutex> lock(mtx_);

      // host_ and dev_ must be defined here as Bind() must have been called and the runloop has not
      // yet been been drained in Unbind().
      ZX_DEBUG_ASSERT(host_);
      ZX_DEBUG_ASSERT(dev_);

      if (!success) {
        bt_log(ERROR, "bt-host", "failed to initialize adapter; cleaning up");
        device_init_reply(dev_, ZX_ERR_INTERNAL, nullptr /* No arguments */);
        // DDK will call Unbind here to clean up.
      } else {
        bt_log(DEBUG, "bt-host", "adapter initialized; make device visible");
        host_->gatt_host()->SetRemoteServiceWatcher(
            fit::bind_member(this, &HostDevice::OnRemoteGattServiceAdded));
        device_init_reply(dev_, ZX_OK, nullptr /* No arguments */);
        return;
      }
    });
  });
}

void HostDevice::Unbind() {
  bt_log(DEBUG, "bt-host", "unbind");

  {
    std::lock_guard<std::mutex> lock(mtx_);

    // Do this immediately to stop receiving new service callbacks.
    bt_log(TRACE, "bt-host", "removing GATT service watcher");
    host_->gatt_host()->SetRemoteServiceWatcher({});

    async::PostTask(loop_.dispatcher(), [this] {
      std::lock_guard<std::mutex> lock(mtx_);
      host_->ShutDown();
      host_ = nullptr;
      loop_.Quit();
    });

    // Don't hold lock waiting on the loop to terminate.
  }

  // Make sure that the ShutDown task runs before this returns. We re
  bt_log(TRACE, "bt-host", "waiting for shut down tasks to complete");
  loop_.JoinThreads();

  device_unbind_reply(dev_);

  bt_log(DEBUG, "bt-host", "GAP has been shut down");
}

void HostDevice::Release() {
  bt_log(DEBUG, "bt-host", "release");
  delete this;
}

zx_status_t HostDevice::OpenHostChannel(zx::channel channel) {
  ZX_DEBUG_ASSERT(channel);
  std::lock_guard<std::mutex> lock(mtx_);

  // This is called from the fidl operation OpenChannelOp.  No fidl calls will be delivered to the
  // driver before dev_ is initialized by Bind() nor host_ initialized by Init(), and no fidl calls
  // will be delivered after the DDK calls Unbind() and host_ is removed.
  ZX_DEBUG_ASSERT(host_);
  ZX_DEBUG_ASSERT(dev_);

  // Tell Host to start processing messages on this handle.
  async::PostTask(loop_.dispatcher(), [host = host_, chan = std::move(channel)]() mutable {
    host->BindHostInterface(std::move(chan));
  });

  return ZX_OK;
}

void HostDevice::OnRemoteGattServiceAdded(bt::gatt::PeerId peer_id,
                                          fbl::RefPtr<bt::gatt::RemoteService> service) {
  TRACE_DURATION("bluetooth", "HostDevice::OnRemoteGattServiceAdded");

  // Only publish children for HID-over-GATT.
  if (service->uuid() != kHogUuid) {
    return;
  }

  std::lock_guard<std::mutex> lock(mtx_);

  // This is run on the host event loop. Bind(), Init() and Unbind() should maintain the invariant
  // that dev_ and host_ are initialized when the event loop is running.
  ZX_DEBUG_ASSERT(host_);
  ZX_DEBUG_ASSERT(dev_);

  __UNUSED zx_status_t status = GattRemoteServiceDevice::Publish(dev_, peer_id, service);
}

}  // namespace bthost
