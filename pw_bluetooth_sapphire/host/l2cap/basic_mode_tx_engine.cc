// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_tx_engine.h"

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::l2cap::internal {

bool BasicModeTxEngine::QueueSdu(ByteBufferPtr sdu) {
  BT_ASSERT(sdu);
  if (sdu->size() > max_tx_sdu_size_) {
    bt_log(DEBUG, "l2cap", "SDU size exceeds channel TxMTU (channel-id: %#.4x)", channel_id_);
    return false;
  }
  send_frame_callback_(std::move(sdu));
  return true;
}

}  // namespace bt::l2cap::internal
