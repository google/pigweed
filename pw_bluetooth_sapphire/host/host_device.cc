// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "host_device.h"

#include <zircon/status.h>

#include "host.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/device_wrapper.h"

namespace bthost {

HostDevice::HostDevice(zx_device_t* device)
    : dev_(nullptr), parent_(device), loop_(&kAsyncLoopConfigNoAttachToCurrentThread) {
  ZX_DEBUG_ASSERT(parent_);

  dev_proto_.version = DEVICE_OPS_VERSION;
  dev_proto_.unbind = &HostDevice::DdkUnbind;
  dev_proto_.release = &HostDevice::DdkRelease;
  dev_proto_.message = &HostDevice::DdkMessage;
}

zx_status_t HostDevice::Bind() {
  bt_log(TRACE, "bt-host", "bind");

  std::lock_guard<std::mutex> lock(mtx_);

  bt_hci_protocol_t hci_proto;
  zx_status_t status = device_get_protocol(parent_, ZX_PROTOCOL_BT_HCI, &hci_proto);
  if (status != ZX_OK) {
    bt_log(ERROR, "bt-host", "failed to obtain bt-hci protocol ops: %s",
           zx_status_get_string(status));
    return status;
  }

  if (!hci_proto.ops) {
    bt_log(ERROR, "bt-host", "bt-hci device ops required!");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto.ops->open_command_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_command_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto.ops->open_acl_data_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_acl_data_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  if (!hci_proto.ops->open_snoop_channel) {
    bt_log(ERROR, "bt-host", "bt-hci op required: open_snoop_channel");
    return ZX_ERR_NOT_SUPPORTED;
  }

  // We are required to publish a device before returning from Bind but we
  // haven't fully initialized the adapter yet. We create the bt-host device as
  // invisible until initialization completes on the host thread. We also
  // disallow other drivers from directly binding to it.
  device_add_args_t args = {
      .version = DEVICE_ADD_ARGS_VERSION,
      .name = "bt_host",
      .ctx = this,
      .ops = &dev_proto_,
      .proto_id = ZX_PROTOCOL_BT_HOST,
      .flags = DEVICE_ADD_NON_BINDABLE | DEVICE_ADD_INVISIBLE,
  };

  status = device_add(parent_, &args, &dev_);
  if (status != ZX_OK) {
    bt_log(ERROR, "bt-host", "Failed to publish device: %s", zx_status_get_string(status));
    return status;
  }

  loop_.StartThread("bt-host (gap)");

  // Send the bootstrap message to Host. The Host object can only be accessed on
  // the Host thread.
  async::PostTask(loop_.dispatcher(), [hci_proto, this] {
    bt_log(SPEW, "bt-host", "host thread start");

    std::lock_guard<std::mutex> lock(mtx_);
    host_ = fxl::MakeRefCounted<Host>(hci_proto);
    host_->Initialize([this](bool success) {
      {
        std::lock_guard<std::mutex> lock(mtx_);

        // Abort if CleanUp has been called.
        if (!host_) {
          bt_log(SPEW, "bt-host", "host already removed; nothing to do");
          return;
        }

        if (success) {
          bt_log(TRACE, "bt-host", "adapter initialized; make device visible");
          host_->gatt_host()->SetRemoteServiceWatcher(
              fit::bind_member(this, &HostDevice::OnRemoteGattServiceAdded));
          device_make_visible(dev_, nullptr);
          return;
        }

        bt_log(ERROR, "bt-host", "failed to initialize adapter; cleaning up");

        host_->ShutDown();
        CleanUp();
      }

      loop_.JoinThreads();
    });
  });

  return ZX_OK;
}

void HostDevice::Unbind() {
  bt_log(TRACE, "bt-host", "unbind");

  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!host_) {
      bt_log(SPEW, "bt-host", "host already removed");
      return;
    }

    // Do this immediately to stop receiving new service callbacks.
    bt_log(SPEW, "bt-host", "removing GATT service watcher");
    host_->gatt_host()->SetRemoteServiceWatcher({});

    // Tear down the bt-host device and all of its GATT children. Make a copy of
    // |host_| first since CleanUp() clears it.
    auto host = host_;
    CleanUp();

    async::PostTask(loop_.dispatcher(), [this, host] {
      host->ShutDown();
      loop_.Quit();
    });

    // Don't hold lock waiting on the loop to terminate.
  }

  // Make sure that the ShutDown task runs before this returns. We re
  bt_log(SPEW, "bt-host", "waiting for shut down tasks to complete");
  loop_.JoinThreads();

  bt_log(TRACE, "bt-host", "GAP has been shut down");
}

void HostDevice::Release() {
  bt_log(TRACE, "bt-host", "release");
  delete this;
}

zx_status_t HostDevice::OpenHostChannel(zx::channel channel) {
  ZX_DEBUG_ASSERT(channel);
  std::lock_guard<std::mutex> lock(mtx_);

  // It's possible that this is being processed whilst the device is unbinding,
  // in which case |host_| can be null
  if (!host_) {
    bt_log(ERROR, "bt-host", "Cannot open channel, host is unbound");
    return ZX_ERR_BAD_STATE;
  }

  // Tell Host to start processing messages on this handle.
  ZX_DEBUG_ASSERT(host_);
  async::PostTask(loop_.dispatcher(), [host = host_, chan = std::move(channel)]() mutable {
    host->BindHostInterface(std::move(chan));
  });

  return ZX_OK;
}

void HostDevice::OnRemoteGattServiceAdded(bt::gatt::PeerId peer_id,
                                          fbl::RefPtr<bt::gatt::RemoteService> service) {
  std::lock_guard<std::mutex> lock(mtx_);

  // Ignore the event if our bt-host device has been removed (this is likely during shut down).
  if (!dev_) {
    return;
  }

  auto gatt_device = std::make_unique<GattRemoteServiceDevice>(peer_id, service);
  zx_status_t status = gatt_device->Bind(dev_);
  if (status != ZX_OK) {
    return;
  }

  auto ptr = gatt_device.release();
  gatt_devices_.insert(ptr);
  service->AddRemovedHandler([this, ptr] {
    std::lock_guard<std::mutex> lock(mtx_);

    auto iter = gatt_devices_.find(ptr);
    if (iter == gatt_devices_.end()) {
      // This can happen if the child was already removed in HostDevice::CleanUp() as a result of
      // the HCI device unbind sequence.
      bt_log(SPEW, "bt-host", "bt-gatt-svc child already unpublished");
      return;
    }

    ptr->Unbind();
    gatt_devices_.erase(iter);
  });
}

void HostDevice::CleanUp() {
  bt_log(TRACE, "bt-host", "clean up");

  // Explicitly unbind all published bt-gatt-svc children.
  auto children = std::move(gatt_devices_);
  for (auto* child : children) {
    child->Unbind();
  }

  host_ = nullptr;

  if (dev_) {
    device_async_remove(dev_);
    dev_ = nullptr;
  }
}

}  // namespace bthost
