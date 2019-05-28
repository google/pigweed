// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <memory>

#include "host_device.h"

zx_status_t bt_host_bind(void* ctx, zx_device_t* device) {
  auto dev = std::make_unique<bthost::HostDevice>(device);
  zx_status_t status = dev->Bind();
  if (status == ZX_OK) {
    // devmgr is now in charge of the memory for |dev|.
    dev.release();
  }
  return status;
}

static constexpr zx_driver_ops_t bt_host_driver_ops = []() {
    zx_driver_ops_t ops = {};
    ops.version = DRIVER_OPS_VERSION;
    ops.bind = bt_host_bind;
    return ops;
}();

// clang-format off
ZIRCON_DRIVER_BEGIN(bt_host, bt_host_driver_ops, "fuchsia", "0.1", 1)
  BI_MATCH_IF(EQ, BIND_PROTOCOL, ZX_PROTOCOL_BT_HCI),
ZIRCON_DRIVER_END(bt_host)
