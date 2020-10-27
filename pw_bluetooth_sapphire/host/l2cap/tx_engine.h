// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TX_ENGINE_H_

#include <lib/fit/function.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

// The interface between a Channel, and the module implementing the
// mode-specific transmit logic. The primary purposes of an TxEngine are a) to
// transform SDUs into PDUs, and b) to transmit/retransmit the PDUs at the
// appropriate time. See Bluetooth Core Spec v5.0, Volume 3, Part A, Sec 2.4,
// "Modes of Operation" for more information about the possible modes.
class TxEngine {
 public:
  // Type defining the callback that a TxEngine uses to deliver a PDU to lower
  // layers. The callee may assume that the ByteBufferPtr owns an instance of a
  // DynamicByteBuffer or SlabBuffer.
  using SendFrameCallback = fit::function<void(ByteBufferPtr pdu)>;

  // Creates a transmit engine, which will invoke |send_frame_callback|
  // when a PDU is ready for transmission. This callback may be invoked
  // synchronously from QueueSdu(), as well as asynchronously (e.g. when a
  // retransmission timer expires).
  //
  // NOTE: The user of this class must ensure that a synchronous invocation of
  // |send_frame_callback| does not deadlock. E.g., the callback must not
  // attempt to lock the same mutex as the caller of QueueSdu().
  TxEngine(ChannelId channel_id, uint16_t max_tx_sdu_size, SendFrameCallback send_frame_callback)
      : channel_id_(channel_id),
        max_tx_sdu_size_(max_tx_sdu_size),
        send_frame_callback_(std::move(send_frame_callback)) {
    ZX_ASSERT(max_tx_sdu_size_);
  }
  virtual ~TxEngine() = default;

  // Queues an SDU for transmission, returning true on success.
  //
  // * As noted in the ctor documentation, this _may_ result in a synchronous
  //   invocation of |send_frame_callback_|.
  // * It is presumed that the ByteBufferPtr owns an instance of a
  //   DynamicByteBuffer or SlabBuffer.
  virtual bool QueueSdu(ByteBufferPtr) = 0;

 protected:
  const ChannelId channel_id_;
  const uint16_t max_tx_sdu_size_;
  // Invoked when a PDU is ready for transmission.
  const SendFrameCallback send_frame_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TxEngine);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TX_ENGINE_H_
