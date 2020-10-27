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

// Wrapper for an L2CAP channel which manages pairing. Exists so that the channel callbacks can be
// changed at runtime, which is done by changing the PairingChannel::Handler pointer.

class PairingChannel {
 public:
  // Interface for receiving L2CAP channel events.
  class Handler {
   public:
    virtual ~Handler() = default;
    virtual void OnRxBFrame(ByteBufferPtr) = 0;
    virtual void OnChannelClosed() = 0;
  };

  // Initializes this PairingChannel with the L2CAP SMP fixed channel that this class wraps.
  explicit PairingChannel(fbl::RefPtr<l2cap::Channel> chan);

  // For setting the new handler, expected to be used when switching phases. PairingChannel is not
  // fully initialized until SetChannelHandler has been called with a valid Handler. This two-phase
  // initialization exists because concrete Handlers are expected to depend on PairingChannels.
  void SetChannelHandler(fxl::WeakPtr<Handler> new_handler);

  fxl::WeakPtr<PairingChannel> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  bool SupportsSecureConnections() const {
    return chan_->max_rx_sdu_size() >= kLeSecureConnectionsMtu &&
           chan_->max_tx_sdu_size() >= kLeSecureConnectionsMtu;
  }

  // Convenience functions to provide direct access to the underlying l2cap::Channel.
  l2cap::Channel* get() const { return chan_.get(); }
  l2cap::Channel* operator->() const { return get(); }

  ~PairingChannel() = default;

  // Wrapper which abstracts some of the boilerplate around sending a SMP object.
  template <typename PayloadType>
  void SendMessage(Code message_code, const PayloadType& payload) {
    auto kExpectedSize = kCodeToPayloadSize.find(message_code);
    ZX_ASSERT(kExpectedSize != kCodeToPayloadSize.end());
    ZX_ASSERT(sizeof(PayloadType) == kExpectedSize->second);
    auto pdu = util::NewPdu(sizeof(PayloadType));
    PacketWriter writer(message_code, pdu.get());
    *writer.mutable_payload<PayloadType>() = payload;
    chan_->Send(std::move(pdu));
  }

 private:
  // Used to delegate the L2CAP callbacks to the current handler
  void OnRxBFrame(ByteBufferPtr ptr);
  void OnChannelClosed();

  // The l2cap Channel this class wraps. Uses a ScopedChannel because a PairingChannel is expected
  // to own the lifetime of the underlying L2CAP channel.
  l2cap::ScopedChannel chan_;

  // L2CAP channel events are delegated to this handler.
  fxl::WeakPtr<Handler> handler_;

  fxl::WeakPtrFactory<PairingChannel> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingChannel);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_CHANNEL_H_
