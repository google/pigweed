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

Engine::EnhancedRetransmissionModeTxEngine(ChannelId channel_id, uint16_t max_tx_sdu_size,
                                           uint8_t max_transmissions, uint8_t n_frames_in_tx_window,
                                           SendFrameCallback send_frame_callback,
                                           ConnectionFailureCallback connection_failure_callback)
    : TxEngine(channel_id, max_tx_sdu_size, std::move(send_frame_callback)),
      max_transmissions_(max_transmissions),
      n_frames_in_tx_window_(n_frames_in_tx_window),
      connection_failure_callback_(std::move(connection_failure_callback)),
      expected_ack_seq_(0),
      next_tx_seq_(0),
      last_tx_seq_(0),
      req_seqnum_(0),
      retransmitted_during_poll_(false),
      n_receiver_ready_polls_sent_(0),
      remote_is_busy_(false) {
  ZX_DEBUG_ASSERT(n_frames_in_tx_window_);
  receiver_ready_poll_task_.set_handler([this] {
    ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
    SendReceiverReadyPoll();
    StartMonitorTimer();
  });
  monitor_task_.set_handler([this] {
    ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
    if (max_transmissions_ == 0 || n_receiver_ready_polls_sent_ < max_transmissions_) {
      SendReceiverReadyPoll();
      StartMonitorTimer();
    } else {
      connection_failure_callback_();  // May invalidate |self|.
    }
  });
}

bool Engine::QueueSdu(ByteBufferPtr sdu) {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_ASSERT(sdu);
  // TODO(BT-440): Add support for segmentation
  if (sdu->size() > max_tx_sdu_size_) {
    bt_log(DEBUG, "l2cap", "SDU size exceeds channel TxMTU (channel-id: %#.4x)", channel_id_);
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

void Engine::UpdateAckSeq(uint8_t new_seq, bool is_poll_response) {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  // TODO(quiche): Reconsider this assertion if we allow reconfiguration of the TX window.
  ZX_DEBUG_ASSERT_MSG(NumUnackedFrames() <= n_frames_in_tx_window_,
                      "(NumUnackedFrames() = %u, n_frames_in_tx_window_ = %u, "
                      "expected_ack_seq_ = %u, last_tx_seq_ = %u)",
                      NumUnackedFrames(), n_frames_in_tx_window_, expected_ack_seq_, last_tx_seq_);

  const auto n_frames_acked = NumFramesBetween(expected_ack_seq_, new_seq);
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

  expected_ack_seq_ = new_seq;
  if (expected_ack_seq_ == next_tx_seq_) {
    receiver_ready_poll_task_.Cancel();
  }

  const auto range_request = std::exchange(range_request_, std::nullopt);

  // RemoteBusy is cleared as the first action to take when receiving a REJ per Core Spec v5.0 Vol
  // 3, Part A, Sec 8.6.5.9–11, so their corresponding member variables shouldn't be both set.
  ZX_ASSERT(!(range_request.has_value() && remote_is_busy_));
  bool should_retransmit = range_request.has_value();

  // This implements the logic for RejActioned in the Recv {I,RR,REJ} (F=1) event for all of the
  // receiver states (Core Spec v5.0 Vol 3, Part A, Sec 8.6.5.9–11).
  if (is_poll_response) {
    monitor_task_.Cancel();
    if (retransmitted_during_poll_) {
      should_retransmit = false;
      retransmitted_during_poll_ = false;
    } else {
      should_retransmit = true;
    }
  }

  if (remote_is_busy_) {
    return;
  }

  // This implements the logic for PbitOutstanding in the Recv REJ (F=0) event for all of the
  // receiver states (Core Spec v5.0 Vol 3, Part A, Sec 8.6.5.9–11).
  if (range_request.has_value() && !is_poll_response && monitor_task_.is_pending()) {
    retransmitted_during_poll_ = true;
  }

  if (should_retransmit) {
    monitor_task_.Cancel();
    const bool set_is_poll_response = range_request.value_or(RangeRequest{}).is_poll_request;
    if (RetransmitUnackedData(set_is_poll_response).is_error()) {
      return;
    }
  }

  MaybeSendQueuedData();

  // TODO(quiche): Restart the receiver_ready_poll_task_, if there's any
  // remaining unacknowledged data.
}

void Engine::UpdateReqSeq(uint8_t new_seq) {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  req_seqnum_ = new_seq;
}

void Engine::ClearRemoteBusy() {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  // TODO(quiche): Maybe clear backpressure on the Channel (subject to TxWindow contraints).
  remote_is_busy_ = false;
}

void Engine::SetRemoteBusy() {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  // TODO(BT-774): Signal backpressure to the Channel.
  remote_is_busy_ = true;
  receiver_ready_poll_task_.Cancel();
}

void Engine::SetRangeRetransmit(bool is_poll_request) {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
  // Store REJ state for UpdateAckSeq to handle.
  range_request_ = RangeRequest{.is_poll_request = is_poll_request};
}

void Engine::MaybeSendQueuedData() {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());
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
  auto it = std::find_if(pending_pdus_.begin(), pending_pdus_.end(),
                         [](const auto& pending_pdu) { return pending_pdu.tx_count == 0; });

  while (it != pending_pdus_.end() && NumUnackedFrames() < n_frames_in_tx_window_) {
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
                                        kErtmReceiverReadyPollTimerDuration);
}

void Engine::StartMonitorTimer() {
  ZX_DEBUG_ASSERT(!receiver_ready_poll_task_.is_pending());
  monitor_task_.Cancel();
  monitor_task_.PostDelayed(async_get_default_dispatcher(), kErtmMonitorTimerDuration);
}

void Engine::SendReceiverReadyPoll() {
  SimpleReceiverReadyFrame frame;
  frame.set_receive_seq_num(req_seqnum_);
  frame.set_is_poll_request();
  ++n_receiver_ready_polls_sent_;
  ZX_ASSERT_MSG(max_transmissions_ == 0 || n_receiver_ready_polls_sent_ <= max_transmissions_,
                "(n_receiver_ready_polls_sent_ = %u, "
                "max_transmissions = %u)",
                n_receiver_ready_polls_sent_, max_transmissions_);
  send_frame_callback_(std::make_unique<DynamicByteBuffer>(BufferView(&frame, sizeof(frame))));
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
    return NumFramesBetween(expected_ack_seq_,
                            last_tx_seq_ + 1  // Include frame with |last_tx_seq_| in count
    );
  }
}

void Engine::SendPdu(PendingPdu* pdu) {
  ZX_DEBUG_ASSERT(pdu);
  pdu->buf.AsMutable<SimpleInformationFrameHeader>().set_receive_seq_num(req_seqnum_);
  pdu->tx_count++;
  StartReceiverReadyPollTimer();
  send_frame_callback_(std::make_unique<DynamicByteBuffer>(pdu->buf));
}

fit::result<> Engine::RetransmitUnackedData(bool set_is_poll_response) {
  // The receive engine should have cleared the remote busy condition before
  // calling any method that would cause us (the transmit engine) to retransmit
  // unacked data. See, e.g., Core Spec v5.0, Volume 3, Part A, Table 8.6, row
  // "Recv REJ (F=0)".
  ZX_DEBUG_ASSERT(!remote_is_busy_);

  const auto n_to_send = NumUnackedFrames();
  ZX_ASSERT(n_to_send <= n_frames_in_tx_window_);
  ZX_DEBUG_ASSERT(n_to_send <= pending_pdus_.size());

  auto cur_frame = pending_pdus_.begin();
  auto last_frame = std::next(cur_frame, n_to_send);
  while (cur_frame != last_frame) {
    ZX_DEBUG_ASSERT(cur_frame != pending_pdus_.end());

    if (cur_frame->tx_count >= max_transmissions_) {
      ZX_ASSERT_MSG(cur_frame->tx_count == max_transmissions_, "%hhu != %hhu", cur_frame->tx_count,
                    max_transmissions_);
      connection_failure_callback_();
      return fit::error();
    }

    const auto control_field = cur_frame->buf.As<EnhancedControlField>();
    if (set_is_poll_response) {
      cur_frame->buf.AsMutable<EnhancedControlField>().set_is_poll_response();

      // Per "Retransmit-I-frames" of Core Spec v5.0 Vol 3, Part A, Sec 8.6.5.6, "[t]he F-bit of all
      // other [than the first] unacknowledged I-frames sent shall be 0," so clear this for
      // subsequent iterations.
      set_is_poll_response = false;
    }

    // TODO(BT-860): If the task is already running, we should not restart it.
    SendPdu(&*cur_frame);
    cur_frame->buf.AsMutable<EnhancedControlField>() = control_field;
    ++cur_frame;
  }

  return fit::ok();
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
