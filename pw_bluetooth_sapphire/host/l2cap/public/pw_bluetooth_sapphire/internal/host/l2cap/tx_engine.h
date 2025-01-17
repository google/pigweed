// Copyright 2023 The Pigweed Authors
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
#include <lib/fit/function.h>
#include <pw_assert/check.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

// The interface between a Channel, and the module implementing the
// mode-specific transmit logic. The primary purposes of an TxEngine are a) to
// transform SDUs into PDUs, and b) to transmit/retransmit the PDUs at the
// appropriate time. See Bluetooth Core Spec v5.0, Volume 3, Part A, Sec 2.4,
// "Modes of Operation" for more information about the possible modes.
class TxEngine {
 public:
  // The interface of a transmission channel, which should be ChannelImpl in
  // production. The channel is able to send frames over the channel with
  // SendFrame as well as retrieve SDUs that are queued for sending in the
  // channel.
  class TxChannel {
   public:
    virtual ~TxChannel() = default;
    // Callback that a TxEngine uses to deliver a PDU to lower layers. The
    // callee may assume that the ByteBufferPtr owns an instance of a
    // DynamicByteBuffer or SlabBuffer.
    virtual void SendFrame(ByteBufferPtr pdu) = 0;
    // Callback that a TxEngine uses to retrieve the next SDU queued in the
    // channel. If the queue is empty, this callback should return
    // std::nullopt, and the channel must notify the TxEngine when an SDU
    // becomes available by calling NotifySduQueued().
    virtual std::optional<ByteBufferPtr> GetNextQueuedSdu() = 0;
  };

  // Creates a transmit engine, which will process SDUs into PDUs for
  // transmission.
  //
  // The engine will invoke the channel's SendFrame() handler when a PDU is
  // ready for transmission. This callback may be invoked synchronously from
  // NotifySduQueued(), as well as asynchronously (e.g. when a retransmission
  // timer expires).
  //
  // NOTE(Lifetime): The TxChannel must outlive the TxEngine.
  // NOTE(Deadlock): The user of this class must ensure that a synchronous
  //   invocation of SendFrame() does not deadlock. E.g., the callback must not
  //   attempt to lock the same mutex as the caller of NotifySduQueued().
  TxEngine(ChannelId channel_id, uint16_t max_tx_sdu_size, TxChannel& channel)
      : channel_id_(channel_id),
        max_tx_sdu_size_(max_tx_sdu_size),
        channel_(channel) {
    PW_CHECK(max_tx_sdu_size_);
  }
  virtual ~TxEngine() = default;

  // Notify the TxEngine that an SDU should be available for it to process.
  //
  // NOTE(Deadlock): As noted in the ctor documentation, this _may_ result in
  //   the synchronous invocation of channel_.SendFrame().
  virtual void NotifySduQueued() = 0;

 protected:
  ChannelId channel_id() const { return channel_id_; }
  uint16_t max_tx_sdu_size() const { return max_tx_sdu_size_; }
  const TxChannel& channel() const { return channel_; }
  TxChannel& channel() { return channel_; }

 private:
  const ChannelId channel_id_;
  const uint16_t max_tx_sdu_size_;
  TxChannel& channel_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TxEngine);
};

}  // namespace bt::l2cap::internal
