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

namespace {

// Returns the number of frames within the range from |low| to |high|, inclusive
// of |low|, but exclusive of |high|. Returns zero if |low == high|.
uint8_t NumFramesBetween(uint8_t low, uint8_t high) {
  if (high < low) {
    high += (EnhancedControlField::kMaxSeqNum + 1);
  }
  return high - low;
}

}  // namespace

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
      next_tx_seq_(0),
      last_tx_seq_(0),
      req_seqnum_(0),
      n_receiver_ready_polls_sent_(0),
      remote_is_busy_(false),
      weak_factory_(this) {
  ZX_DEBUG_ASSERT(n_frames_in_tx_window_);
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

  const auto seq_num = GetNextTxSeq();
  SimpleInformationFrameHeader header(seq_num);
  DynamicByteBuffer frame(sizeof(header) + sdu->size());
  auto body = frame.mutable_view(sizeof(header));
  frame.WriteObj(header);
  sdu->Copy(&body);

  // TODO(BT-773): Limit the size of the queue.
  pending_pdus_.push_back(std::move(frame));
  MaybeSendQueuedData();
  return true;
}

void Engine::UpdateAckSeq(uint8_t new_seq, bool is_final) {
  // TODO(quiche): Reconsider this assertion if we allow reconfiguration of
  // the TX window.
  ZX_DEBUG_ASSERT_MSG(NumUnackedFrames() <= n_frames_in_tx_window_,
                      "(NumUnackedFrames() = %u, n_frames_in_tx_window_ = %u, "
                      "ack_seqnum_ = %u, last_tx_seq_ = %u)",
                      NumUnackedFrames(), n_frames_in_tx_window_, ack_seqnum_,
                      last_tx_seq_);

  const auto n_frames_acked = NumFramesBetween(ack_seqnum_, new_seq);
  if (n_frames_acked > NumUnackedFrames()) {
    // TODO(quiche): Consider the best error handling strategy here. Should we
    // drop the connection entirely?
    return;
  }

  auto n_frames_to_discard = n_frames_acked;
  while (n_frames_to_discard) {
    ZX_DEBUG_ASSERT(!pending_pdus_.empty());
    pending_pdus_.pop_front();
    --n_frames_to_discard;
  }

  ack_seqnum_ = new_seq;
  if (ack_seqnum_ == next_tx_seq_) {
    receiver_ready_poll_task_.Cancel();
  }

  if (is_final) {
    monitor_task_.Cancel();
  }

  auto self = this;
  if (is_final && !remote_is_busy_) {
    self = RetransmitUnackedData();
  }

  if (self) {
    MaybeSendQueuedData();

    // TODO(quiche): Restart the receiver_ready_poll_task_, if there's any
    // remaining unacknowledged data.
  }
}

void Engine::UpdateReqSeq(uint8_t new_seq) { req_seqnum_ = new_seq; }

void Engine::ClearRemoteBusy() {
  // TODO(quiche): Maybe clear backpressure on the Channel (subject to TxWindow
  // contraints).
  remote_is_busy_ = false;
}

void Engine::SetRemoteBusy() {
  // TODO(BT-774): Signal backpressure to the Channel.
  remote_is_busy_ = true;
  receiver_ready_poll_task_.Cancel();
}

void Engine::MaybeSendQueuedData() {
  if (remote_is_busy_ || monitor_task_.is_pending()) {
    return;
  }

  // Find the first PDU that has not already been transmitted (if any).
  // * This is not necessarily the first PDU, because that may have been
  //   transmited already, and is just pending acknowledgement.
  // * This is not necessarily the last PDU, because earlier PDUs may have been
  //   queued without having been sent over-the-air (due, e.g., to tx_window
  //   constraints).
  //
  // TODO(quiche): Consider if there's a way to do this that isn't O(n).
  auto it = std::find_if(
      pending_pdus_.begin(), pending_pdus_.end(),
      [](const auto& pending_pdu) { return pending_pdu.tx_count == 0; });

  while (it != pending_pdus_.end() &&
         NumUnackedFrames() < n_frames_in_tx_window_) {
    ZX_DEBUG_ASSERT(it->tx_count == 0);
    SendPdu(&*it);
    last_tx_seq_ = it->buf.As<SimpleInformationFrameHeader>().tx_seq();
    ++it;
  }
}

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

uint8_t Engine::GetNextTxSeq() {
  auto ret = next_tx_seq_;
  ++next_tx_seq_;
  if (next_tx_seq_ > EnhancedControlField::kMaxSeqNum) {
    next_tx_seq_ = 0;
  }
  return ret;
}

uint8_t Engine::NumUnackedFrames() {
  if (pending_pdus_.empty()) {
    // Initially, |ack_seqnum_ == last_tx_seq_ == 0|, but the number of
    // unacknowledged frames is 0, not 1.
    return 0;
  } else if (pending_pdus_.front().tx_count == 0) {
    // While we have some data queued, none of that data has been sent
    // over-the-air. This might happen, e.g., transiently in QueueSdu().
    return 0;
  } else {
    // Having ascertained that some data _is_ in flight, the number of frames in
    // flight is given by the expression below.
    return NumFramesBetween(
        ack_seqnum_,
        last_tx_seq_ + 1  // Include frame with |last_tx_seq_| in count
    );
  }
}

void Engine::SendPdu(PendingPdu* pdu) {
  ZX_DEBUG_ASSERT(pdu);
  pdu->buf.AsMutable<SimpleInformationFrameHeader>().set_request_seq_num(
      req_seqnum_);
  pdu->tx_count++;
  StartReceiverReadyPollTimer();
  send_basic_frame_callback_(std::make_unique<DynamicByteBuffer>(pdu->buf));
}

Engine* Engine::RetransmitUnackedData() {
  // The receive engine should have cleared the remote busy condition before
  // calling any method that would cause us (the transmit engine) to retransmit
  // unacked data. See, e.g., Core Spec v5.0, Volume 3, Part A, Table 8.6, row
  // "Recv REJ (F=0)".
  ZX_DEBUG_ASSERT(!remote_is_busy_);

  auto n_to_send = std::min(n_frames_in_tx_window_, NumUnackedFrames());
  ZX_DEBUG_ASSERT(n_to_send <= pending_pdus_.size());

  auto cur_frame = pending_pdus_.begin();
  auto last_frame = std::next(cur_frame, n_to_send);
  while (cur_frame != last_frame) {
    ZX_DEBUG_ASSERT(cur_frame != pending_pdus_.end());

    if (cur_frame->tx_count >= max_transmissions_) {
      ZX_DEBUG_ASSERT(cur_frame->tx_count == max_transmissions_);
      connection_failure_callback_();
      return nullptr;
    }

    // TODO(BT-860): If the task is already running, we should not restart it.
    SendPdu(&*cur_frame);
    ++cur_frame;
  }

  return this;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
