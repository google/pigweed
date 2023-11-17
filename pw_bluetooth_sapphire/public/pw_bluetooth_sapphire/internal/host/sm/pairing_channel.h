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
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/scoped_channel.h"
#include "pw_bluetooth_sapphire/internal/host/sm/packet.h"
#include "pw_bluetooth_sapphire/internal/host/sm/util.h"

namespace bt::sm {

// Bridge class for the SMP L2CAP channel, which implements SM-specific
// functionality on top of existing L2CAP functionality. Besides this
// SM-specific functionality, also allows runtime modification of L2CAP event
// callbacks by changing the PairingChannel::Handler pointer.

class PairingChannel {
 public:
  // Interface for receiving L2CAP channel events.
  class Handler {
   public:
    virtual ~Handler() = default;
    virtual void OnRxBFrame(ByteBufferPtr) = 0;
    virtual void OnChannelClosed() = 0;

    using WeakPtr = WeakSelf<Handler>::WeakPtr;
  };

  // Initializes this PairingChannel with the L2CAP SMP fixed channel that this
  // class wraps and the specified timer reset method. For use in production
  // code.
  PairingChannel(l2cap::Channel::WeakPtr chan, fit::closure timer_resetter);

  // Initializes this PairingChannel with a no-op timer reset method. Only for
  // use in tests of classes which do not depend on the timer reset behavior.
  explicit PairingChannel(l2cap::Channel::WeakPtr chan);

  // For setting the new handler, expected to be used when switching phases.
  // PairingChannel is not fully initialized until SetChannelHandler has been
  // called with a valid Handler. This two-phase initialization exists because
  // concrete Handlers are expected to depend on PairingChannels.
  void SetChannelHandler(Handler::WeakPtr new_handler);

  // Wrapper which encapsulates some of the boilerplate involved in sending an
  // SMP object.
  template <typename PayloadType>
  void SendMessage(Code message_code, const PayloadType& payload) {
    SendMessageNoTimerReset(message_code, payload);
    reset_timer_();
  }

  // This method exists for situations when we send messages while not pairing
  // (e.g. rejection of pairing), where we do not want to reset the SMP timer
  // upon transmission.
  template <typename PayloadType>
  void SendMessageNoTimerReset(Code message_code, const PayloadType& payload) {
    auto kExpectedSize = kCodeToPayloadSize.find(message_code);
    BT_ASSERT(kExpectedSize != kCodeToPayloadSize.end());
    BT_ASSERT(sizeof(PayloadType) == kExpectedSize->second);
    auto pdu = util::NewPdu(sizeof(PayloadType));
    PacketWriter writer(message_code, pdu.get());
    *writer.mutable_payload<PayloadType>() = payload;
    chan_->Send(std::move(pdu));
  }

  using WeakPtr = WeakSelf<PairingChannel>::WeakPtr;
  PairingChannel::WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

  bool SupportsSecureConnections() const {
    return chan_->max_rx_sdu_size() >= kLeSecureConnectionsMtu &&
           chan_->max_tx_sdu_size() >= kLeSecureConnectionsMtu;
  }

  void SignalLinkError() { chan_->SignalLinkError(); }
  bt::LinkType link_type() const { return chan_->link_type(); }
  ~PairingChannel() = default;

 private:
  // Used to delegate the L2CAP callbacks to the current handler
  void OnRxBFrame(ByteBufferPtr ptr);
  void OnChannelClosed();

  // The L2CAP Channel this class wraps. Uses a ScopedChannel because a
  // PairingChannel is expected to own the lifetime of the underlying L2CAP
  // channel.
  l2cap::ScopedChannel chan_;

  // Per v5.2 Vol. 3 Part H 3.4, "The Security Manager Timer shall be reset when
  // an L2CAP SMP command is queued for transmission". This closure signals this
  // reset to occur.
  fit::closure reset_timer_;

  // L2CAP channel events are delegated to this handler.
  Handler::WeakPtr handler_;

  WeakSelf<PairingChannel> weak_self_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingChannel);
};

}  // namespace bt::sm
