// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_tx_engine.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/frame_headers.h"

namespace bt {
namespace l2cap {
namespace internal {

using Engine = EnhancedRetransmissionModeTxEngine;

bool Engine::QueueSdu(common::ByteBufferPtr sdu) {
  ZX_ASSERT(sdu);
  // TODO(BT-440): Add support for segmentation
  if (sdu->size() > tx_mtu_) {
    bt_log(TRACE, "l2cap", "SDU size exceeds channel TxMTU (channel-id: %#.4x)",
           channel_id_);
    return false;
  }

  SimpleInformationFrameHeader header(GetNextSeqnum());
  auto frame =
      std::make_unique<common::DynamicByteBuffer>(sizeof(header) + sdu->size());
  auto body = frame->mutable_view(sizeof(header));
  frame->WriteObj(header);
  sdu->Copy(&body);

  send_basic_frame_callback_(std::move(frame));
  return true;
}

uint8_t Engine::GetNextSeqnum() {
  auto ret = next_seqnum_;
  ++next_seqnum_;
  if (next_seqnum_ > EnhancedControlField::kMaxSeqNum) {
    next_seqnum_ = 0;
  }
  return ret;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
