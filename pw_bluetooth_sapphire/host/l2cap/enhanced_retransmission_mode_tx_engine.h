// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_

#include <list>

#include "lib/async/cpp/task.h"
#include "lib/fit/function.h"
#include "lib/fit/result.h"
#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/tx_engine.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

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
  using ConnectionFailureCallback = fit::function<void()>;

  // Create a transmit engine.
  //
  // The engine will invoke |send_frame_callback| when a PDU is ready for
  // transmission; see tx_engine.h for further detail.
  //
  // The engine will invoke |connection_failure_callback| when a fatal error
  // occurs on this connection. This callback _may_ occur synchronously. For
  // example, a call to UpdateAckSeq() may synchronously invoke
  // |connection_failure_callback|.
  EnhancedRetransmissionModeTxEngine(ChannelId channel_id, uint16_t max_tx_sdu_size,
                                     uint8_t max_transmissions, uint8_t n_frames_in_tx_window,
                                     SendFrameCallback send_frame_callback,
                                     ConnectionFailureCallback connection_failure_callback);
  ~EnhancedRetransmissionModeTxEngine() override = default;

  bool QueueSdu(ByteBufferPtr sdu) override;

  // Updates the Engine's knowledge of the last frame acknowledged by our peer.
  // The value of |is_poll_response| should reflect the 'F' bit in header of the
  // frame which led to this call.
  //
  // * This _may_ trigger retransmission of previously transmitted data.
  // * This _may_ cause the (initial) transmission of queued data.
  void UpdateAckSeq(uint8_t new_seq, bool is_poll_response);

  // Updates the Engine's knowledge of the next frame we expect to receive from
  // our peer.
  void UpdateReqSeq(uint8_t new_seq);

  // Informs the Engine that the peer is able to receive frames.
  void ClearRemoteBusy();

  // Informs the Engine that the peer is unable to receive additional frames at
  // this time.
  void SetRemoteBusy();

  // Requests that the next UpdateAckSeq always retransmit starting at its |new_seq| parameter, even
  // if it's not a poll response. This path fulfills the requirements of receiving the REJ function.
  //
  // |is_poll_request| should be set if the request to retransmit has its P bit set and will cause
  // the first retransmitted frame to have its |is_poll_response| bit (F bit) set per the
  // Retransmit-I-frames action in Core Spec v5.0 Vol 3, Part A, Sec 8.6.5.6.
  void SetRangeRetransmit(bool is_poll_request);

  // Transmits data that has been queued, but which has never been previously
  // sent to our peer. The transmissions are subject to remote-busy and transmit
  // window constraints.
  void MaybeSendQueuedData();

 private:
  struct PendingPdu {
    PendingPdu(DynamicByteBuffer buf_in) : buf(std::move(buf_in)), tx_count(0){};
    DynamicByteBuffer buf;
    uint8_t tx_count;
  };

  // State of a request from a peer to retransmit a range of unacked data (REJ per Core Spec v5.0,
  // Vol 3, Part A, Sec 8.6.1.2).
  struct RangeRequest {
    // True if the request to retransmit has its P bit set.
    bool is_poll_request;
  };

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

  // Starts the monitor timer.  If already running, the existing timer is
  // cancelled, and a new timer is started.
  //
  // Note that replacing the existing timer is required per Core Spec v5.0,
  // Volume 3, Part A, Section 8.6.5.6, "Start-MonitorTimer".
  void StartMonitorTimer();

  void SendReceiverReadyPoll();

  // Return and consume the next sequence number.
  uint8_t GetNextTxSeq();

  // Returns the number of frames that have been transmitted but not yet
  // acknowledged.
  uint8_t NumUnackedFrames();

  void SendPdu(PendingPdu* pdu);

  // Retransmits frames from |pending_pdus_|. Invokes |connection_failure_callback_| on error and
  // returns fit::error().
  //
  // If |set_is_poll_response| is true, then the first frame to be sent will have its
  // |is_poll_response| field set.
  //
  // Notes:
  // * The caller must ensure that !remote_is_busy_|.
  // * When return value is an error, |this| may be invalid.
  [[nodiscard]] fit::result<> RetransmitUnackedData(bool set_is_poll_response);

  const uint8_t max_transmissions_;
  const uint8_t n_frames_in_tx_window_;

  // Invoked when the connection encounters a fatal error.
  const ConnectionFailureCallback connection_failure_callback_;

  // The sequence number we expect in the next acknowledgement from our peer.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t expected_ack_seq_;

  // The sequence number we will use for the next new outbound I-frame.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t next_tx_seq_;

  // The sequence number of the "newest" transmitted frame.
  //
  // This sequence number is updated when a new frame is transmitted. This
  // excludes cases where a frame is queued but not transmitted (due to transmit
  // window limitations), and cases where a frame is retransmitted.
  //
  // This value is useful for determining the number of frames than are
  // in-flight to our peer (frames that have been transmitted but not
  // acknowledged).
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t last_tx_seq_;  // (AKA TxSeq)

  // The sequence number we expect for the next packet sent _to_ us.
  //
  // We assume that the Extended Window Size option is _not_ enabled. In such
  // cases, the sequence number is a 6-bit counter that wraps on overflow. See
  // Core Spec v5.0, Vol 3, Part A, Secs 5.7 and 8.3.
  uint8_t req_seqnum_;

  // Filled by SetRangeRetransmit and cleared by UpdateAckSeq.
  std::optional<RangeRequest> range_request_;

  uint8_t n_receiver_ready_polls_sent_;
  bool remote_is_busy_;
  std::list<PendingPdu> pending_pdus_;
  async::TaskClosure receiver_ready_poll_task_;
  async::TaskClosure monitor_task_;

  fxl::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(EnhancedRetransmissionModeTxEngine);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_TX_ENGINE_H_
