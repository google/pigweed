// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "host_device.h"

extern "C" zx_status_t bt_host_bind(void* ctx, zx_device_t* device) {
  auto dev = std::make_unique<bthost::HostDevice>(device);
  zx_status_t status = dev->Bind();
  if (status == ZX_OK) {
    // devmgr is now in charge of the memory for |dev|.
    dev.release();
  }
  return status;
}
