// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_CHANNEL_H_

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {

// Bridge class for the SMP L2CAP channel, which implements SM-specific functionality on top of
// existing L2CAP functionality. Besides this SM-specific functionality, also allows runtime
// modification of L2CAP event callbacks by changing the PairingChannel::Handler pointer.

class PairingChannel {
 public:
  // Interface for receiving L2CAP channel events.
  class Handler {
   public:
    virtual ~Handler() = default;
    virtual void OnRxBFrame(ByteBufferPtr) = 0;
    virtual void OnChannelClosed() = 0;
  };

  // Initializes this PairingChannel with the L2CAP SMP fixed channel that this class wraps and the
  // specified timer reset method. For use in production code.
  PairingChannel(fxl::WeakPtr<l2cap::Channel> chan, fit::closure timer_resetter);

  // Initializes this PairingChannel with a no-op timer reset method. Only for use in tests of
  // classes which do not depend on the timer reset behavior.
  explicit PairingChannel(fxl::WeakPtr<l2cap::Channel> chan);

  // For setting the new handler, expected to be used when switching phases. PairingChannel is not
  // fully initialized until SetChannelHandler has been called with a valid Handler. This two-phase
  // initialization exists because concrete Handlers are expected to depend on PairingChannels.
  void SetChannelHandler(fxl::WeakPtr<Handler> new_handler);

  // Wrapper which encapsulates some of the boilerplate involved in sending an SMP object.
  template <typename PayloadType>
  void SendMessage(Code message_code, const PayloadType& payload) {
    SendMessageNoTimerReset(message_code, payload);
    reset_timer_();
  }

  // This method exists for situations when we send messages while not pairing (e.g. rejection of
  // pairing), where we do not want to reset the SMP timer upon transmission.
  template <typename PayloadType>
  void SendMessageNoTimerReset(Code message_code, const PayloadType& payload) {
    auto kExpectedSize = kCodeToPayloadSize.find(message_code);
    ZX_ASSERT(kExpectedSize != kCodeToPayloadSize.end());
    ZX_ASSERT(sizeof(PayloadType) == kExpectedSize->second);
    auto pdu = util::NewPdu(sizeof(PayloadType));
    PacketWriter writer(message_code, pdu.get());
    *writer.mutable_payload<PayloadType>() = payload;
    chan_->Send(std::move(pdu));
  }

  fxl::WeakPtr<PairingChannel> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

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

  // The L2CAP Channel this class wraps. Uses a ScopedChannel because a PairingChannel is expected
  // to own the lifetime of the underlying L2CAP channel.
  l2cap::ScopedChannel chan_;

  // Per v5.2 Vol. 3 Part H 3.4, "The Security Manager Timer shall be reset when an L2CAP SMP
  // command is queued for transmission". This closure signals this reset to occur.
  fit::closure reset_timer_;

  // L2CAP channel events are delegated to this handler.
  fxl::WeakPtr<Handler> handler_;

  fxl::WeakPtrFactory<PairingChannel> weak_ptr_factory_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingChannel);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_CHANNEL_H_
