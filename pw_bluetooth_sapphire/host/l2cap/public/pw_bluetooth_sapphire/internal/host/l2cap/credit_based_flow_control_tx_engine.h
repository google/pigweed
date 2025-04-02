// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <deque>

#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/tx_engine.h"

namespace bt::l2cap::internal {

// The transmit half of a credit-based flow control mode connection-oriented
// channel. The "credit" in the credit-based flow control is essentially just
// a counter of how many PDUs can be sent to the peer. The peer will
// occasionally indicate that more credits are available, which allows more
// PDUs to be sent.
class CreditBasedFlowControlTxEngine final : public TxEngine {
 public:
  // Construct a credit-based flow control engine.
  //
  // |channel_id|          - The ID of the channel that packets should be sent
  //                         to.
  // |max_tx_sdu_size|     - Maximum size of an SDU (Tx MTU).
  // |channel|             - A |TxEngine::TxChannel| to send frames to.
  // |mode|                - Mode of credit-based flow control.
  // |max_tx_pdu_payload_size|     - Maximum size of a PDU payload (Tx MPS).
  // |initial_credits|     - Initial value to use for credits. The transmit
  //                         engine can only send packets if it has sufficient
  //                         credits. See |AddCredits|.
  CreditBasedFlowControlTxEngine(ChannelId channel_id,
                                 uint16_t max_tx_sdu_size,
                                 TxChannel& channel,
                                 CreditBasedFlowControlMode mode,
                                 uint16_t max_tx_pdu_payload_size,
                                 uint16_t initial_credits);
  ~CreditBasedFlowControlTxEngine() override;

  // Notify the engine that an SDU is ready for processing. See |TxEngine|.
  void NotifySduQueued() override;

  // Attempt to add credits to the transmit engine. The engine will only
  // transmit pending PDUs if it has sufficient credits to do so. If there
  // are pending PDUs, this function may invoke |SendFrame|.
  //
  // Each enqueued PDU requires a credit to send, and credits can only be
  // replenished through this function.
  //
  // Returns true if successful, or false if the credits value would cause the
  // number of credits to exceed 65535, and does not modify the credit value.
  bool AddCredits(uint16_t credits) override;

  // Provide the current count of unspent credits.
  uint16_t credits() const;
  // Provide the current count of queued segments. This should never exceed
  // |max_tx_sdu_size|/|max_tx_pdu_size|.
  size_t segments_count() const;

 private:
  // Segments a single SDU into one or more PDUs, enqueued in |segments_|
  void SegmentSdu(ByteBufferPtr sdu);
  // Send as many segments as possible (up to the total number of |segments_|),
  // keeping all invariants intact such as the queue of segments remaining and
  // the credit count.
  void TrySendSegments();
  // Dequeue SDUs, segment them, and send until credits are depleted or no more
  // SDUs are available in the queue.
  void ProcessSdus();

  CreditBasedFlowControlMode mode_;
  uint16_t max_tx_pdu_payload_size_;
  uint16_t credits_;

  // An ordered collection of PDUs that make up the segments of the current
  // SDU being sent. May be partial if previous PDUs from the same SDU have
  // been sent, but there are insufficient credits for the remainder.
  std::deque<std::unique_ptr<DynamicByteBuffer>> segments_;
};
}  // namespace bt::l2cap::internal
