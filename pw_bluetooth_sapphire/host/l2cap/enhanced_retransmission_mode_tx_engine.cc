// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_tx_engine.h"

#include <zircon/assert.h>

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/frame_headers.h"

namespace bt {
namespace l2cap {
namespace internal {

using Engine = EnhancedRetransmissionModeTxEngine;

Engine::EnhancedRetransmissionModeTxEngine(
    ChannelId channel_id, uint16_t tx_mtu, uint8_t max_transmissions,
    uint8_t n_frames_in_tx_window,
    SendBasicFrameCallback send_basic_frame_callback,
    ConnectionFailureCallback connection_failure_callback)
    : TxEngine(channel_id, tx_mtu, std::move(send_basic_frame_callback)),
      max_transmissions_(max_transmissions),
      n_frames_in_tx_window_(n_frames_in_tx_window),
      connection_failure_callback_(std::move(connection_failure_callback)),
      ack_seqnum_(0),
      next_seqnum_(0),
      req_seqnum_(0),
      n_receiver_ready_polls_sent_(0),
      weak_factory_(this) {
  receiver_ready_poll_task_.set_handler(
      [weak_self = weak_factory_.GetWeakPtr()](auto dispatcher, auto task,
                                               zx_status_t status) {
        if (status == ZX_OK && weak_self) {
          weak_self->SendReceiverReadyPoll();
          weak_self->StartMonitorTimer();
        }
      });
  monitor_task_.set_handler([weak_self = weak_factory_.GetWeakPtr()](
                                auto dispatcher, auto task,
                                zx_status_t status) {
    if (status == ZX_OK && weak_self) {
      if (weak_self->max_transmissions_ == 0 ||
          weak_self->n_receiver_ready_polls_sent_ <
              weak_self->max_transmissions_) {
        weak_self->SendReceiverReadyPoll();
        weak_self->StartMonitorTimer();
      } else {
        weak_self
            ->connection_failure_callback_();  // May invalidate |weak_self|.
      }
    }
  });
}

bool Engine::QueueSdu(ByteBufferPtr sdu) {
  ZX_ASSERT(sdu);
  // TODO(BT-440): Add support for segmentation
  if (sdu->size() > tx_mtu_) {
    bt_log(TRACE, "l2cap", "SDU size exceeds channel TxMTU (channel-id: %#.4x)",
           channel_id_);
    return false;
  }

  SimpleInformationFrameHeader header(GetNextSeqnum());
  auto frame =
      std::make_unique<DynamicByteBuffer>(sizeof(header) + sdu->size());
  auto body = frame->mutable_view(sizeof(header));
  frame->WriteObj(header);
  sdu->Copy(&body);

  StartReceiverReadyPollTimer();
  send_basic_frame_callback_(std::move(frame));
  return true;
}

void Engine::UpdateAckSeq(uint8_t new_seq, bool is_final) {
  // TODO(quiche): Add a sanity check on the new value. E.g., the new sequence
  // number should probably be within (old value, old value + TxWindow).

  ack_seqnum_ = new_seq;
  if (ack_seqnum_ == next_seqnum_) {
    receiver_ready_poll_task_.Cancel();
  }

  if (is_final) {
    monitor_task_.Cancel();
  }
}

void Engine::UpdateReqSeq(uint8_t new_seq) { req_seqnum_ = new_seq; }

void Engine::StartReceiverReadyPollTimer() {
  ZX_DEBUG_ASSERT(!monitor_task_.is_pending());
  n_receiver_ready_polls_sent_ = 0;
  receiver_ready_poll_task_.Cancel();
  receiver_ready_poll_task_.PostDelayed(async_get_default_dispatcher(),
                                        kReceiverReadyPollTimerDuration);
}

void Engine::StartMonitorTimer() {
  ZX_DEBUG_ASSERT(!receiver_ready_poll_task_.is_pending());
  monitor_task_.Cancel();
  monitor_task_.PostDelayed(async_get_default_dispatcher(),
                            kMonitorTimerDuration);
}

void Engine::SendReceiverReadyPoll() {
  SimpleReceiverReadyFrame frame;
  frame.set_request_seq_num(req_seqnum_);
  frame.set_is_poll_request();
  ++n_receiver_ready_polls_sent_;
  ZX_ASSERT_MSG(max_transmissions_ == 0 ||
                    n_receiver_ready_polls_sent_ <= max_transmissions_,
                "(n_receiver_ready_polls_sent_ = %u, "
                "max_transmissions = %u)",
                n_receiver_ready_polls_sent_, max_transmissions_);
  send_basic_frame_callback_(
      std::make_unique<DynamicByteBuffer>(BufferView(&frame, sizeof(frame))));
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
