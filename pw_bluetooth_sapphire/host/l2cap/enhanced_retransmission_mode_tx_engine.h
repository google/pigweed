// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_

#include "lib/async/cpp/task.h"
#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/tx_engine.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements the sender-side functionality of L2CAP Enhanced Retransmission
// Mode. See Bluetooth Core Spec v5.0, Volume 3, Part A, Sec 2.4, "Modes of
// Operation".
//
// THREAD-SAFETY: This class may is _not_ thread-safe. In particular:
// * the class assumes that some other party ensures that QueueSdu() is not
//   invoked concurrently with the destructor, and
// * the class assumes that all calls to QueueSdu occur on a single thread,
//   for the entire lifetime of an object.
class EnhancedRetransmissionModeTxEngine final : public TxEngine {
 public:
  EnhancedRetransmissionModeTxEngine(
      ChannelId channel_id, uint16_t tx_mtu,
      SendBasicFrameCallback send_basic_frame_callback);
  ~EnhancedRetransmissionModeTxEngine() override = default;

  bool QueueSdu(common::ByteBufferPtr sdu) override;

  // Updates the Engine's knowledge of the last frame acknowledged by our peer.
  // The value of |is_final| should reflect the 'F' bit in header of the frame
  // which led to this call.
  void UpdateAckSeq(uint8_t new_seq, bool is_final);

  // Updates the Engine's knowledge of the next frame we expect to receive from
  // our peer.
  void UpdateReqSeq(uint8_t new_seq);

 private:
  // See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. Note that we assume
  // there is no flush timeout on the underlying logical link.
  //
  // TODO(quiche): This value should be dynamic, and based on the parameters
  // from the L2CAP configuration process. See Core Spec v5.0, Volume 3, Part A,
  // Sec 5.4.
  static constexpr auto kReceiverReadyPollTimerDuration = zx::sec(2);

  // See Core Spec v5.0, Volume 3, Part A, Sec 8.6.2.1. Note that we assume
  // there is no flush timeout on the underlying logical link. If the link
  // _does_ have a flush timeout, then our implementation will be slower to
  // trigger the monitor timeout than the specification recommends.
  //
  // TODO(quiche): This value should be dynamic, and based on the parameters
  // from the L2CAP configuration process. See Core Spec v5.0, Volume 3, Part A,
  // Sec 5.4.
  static constexpr auto kMonitorTimerDuration = zx::sec(12);

  // Starts the receiver ready poll timer. If already running, the existing
  // timer is cancelled, and a new timer is started.
  // Notes:
  // * The specification refers to this as the "retransmission timer". However,
  //   the expiry of this timer doesn't immediately trigger
  //   retransmission. Rather, the timer expiration triggers us asking the
  //   receiver to acknowledge all previously received data. See
  //   "RetransTimer-Expires", in Core Spec v5.0, Volume 3, Part A, Table 8.4.
  // * Replacing the existing timer is required per Core Spec v5.0, Volume 3,
  //   Part A, Section 8.6.5.6, "Start-RetransTimer".
  void StartReceiverReadyPollTimer();

  // Starts the monitor timer. If already running, the existing timer is
  // cancelled, and a new timer is started.
  //
  // Note that replacing the existing timer is required per Core Spec v5.0,
  // Volume 3, Part A, Section 8.6.5.6, "Start-MonitorTimer".
  void StartMonitorTimer();

  void SendReceiverReadyPoll();

  // Return and consume the next sequence number.
  uint8_t GetNextSeqnum();

  // The sequence number we expect in the next acknowledgement from our peer.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t ack_seqnum_;  // (AKA ExpectedAckSeq)

  // The sequence number we will use for the next new outbound I-frame.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t next_seqnum_;  // (AKA NextTxSeq)

  // The sequence number we expect for the next packet sent _to_ us.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t req_seqnum_;

  async::Task receiver_ready_poll_task_;
  async::Task monitor_task_;
  fxl::WeakPtrFactory<EnhancedRetransmissionModeTxEngine>
      weak_factory_;  // Keep last

  FXL_DISALLOW_COPY_AND_ASSIGN(EnhancedRetransmissionModeTxEngine);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
