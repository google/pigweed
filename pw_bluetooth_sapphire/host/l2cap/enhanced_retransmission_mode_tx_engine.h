// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_

#include "src/connectivity/bluetooth/core/bt-host/l2cap/tx_engine.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements the sender-side functionality of L2CAP Enhanced Retransmission
// Mode. See Bluetooth Core Spec v5.0, Volume 3, Part A, Sec 2.4, "Modes of
// Operation".
//
// THREAD-SAFETY: This class may is _not_ thread-safe. In particular, the class
// assumes that some other party ensures that QueueSdu() is not invoked
// concurrently with the destructor.
class EnhancedRetransmissionModeTxEngine final : public TxEngine {
 public:
  EnhancedRetransmissionModeTxEngine(
      ChannelId channel_id, uint16_t tx_mtu,
      SendBasicFrameCallback send_basic_frame_callback)
      : TxEngine(channel_id, tx_mtu, std::move(send_basic_frame_callback)) {}
  ~EnhancedRetransmissionModeTxEngine() override = default;

  bool QueueSdu(common::ByteBufferPtr sdu) override;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(EnhancedRetransmissionModeTxEngine);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
