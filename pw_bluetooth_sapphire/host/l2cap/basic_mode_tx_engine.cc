// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_tx_engine.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace btlib {
namespace l2cap {
namespace internal {

bool BasicModeTxEngine::QueueSdu(common::ByteBufferPtr sdu) {
  ZX_ASSERT(sdu);
  if (sdu->size() > tx_mtu_) {
    bt_log(TRACE, "l2cap", "SDU size exceeds channel TxMTU (channel-id: %#.4x)",
           channel_id_);
    return false;
  }
  send_basic_frame_callback_(std::move(sdu));
  return true;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace btlib
