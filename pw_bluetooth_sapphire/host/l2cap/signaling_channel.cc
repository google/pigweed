// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "signaling_channel.h"

#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <zircon/assert.h>

#include "channel.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

namespace bt {
namespace l2cap {
namespace internal {

SignalingChannel::SignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role)
    : is_open_(true),
      chan_(std::move(chan)),
      role_(role),
      next_cmd_id_(0x01),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(chan_);
  ZX_DEBUG_ASSERT(chan_->id() == kSignalingChannelId || chan_->id() == kLESignalingChannelId);

  // Note: No need to guard against out-of-thread access as these callbacks are
  // called on the L2CAP thread.
  auto self = weak_ptr_factory_.GetWeakPtr();
  chan_->ActivateOnDataDomain(
      [self](ByteBufferPtr sdu) {
        if (self)
          self->OnRxBFrame(std::move(sdu));
      },
      [self] {
        if (self)
          self->OnChannelClosed();
      });
}

SignalingChannel::~SignalingChannel() { ZX_DEBUG_ASSERT(IsCreationThreadCurrent()); }

SignalingChannel::ResponderImpl::ResponderImpl(SignalingChannel* sig, CommandCode code,
                                               CommandId id)
    : sig_(sig), code_(code), id_(id) {
  ZX_DEBUG_ASSERT(sig_);
}

void SignalingChannel::ResponderImpl::Send(const ByteBuffer& rsp_payload) {
  sig()->SendPacket(code_, id_, rsp_payload);
}

void SignalingChannel::ResponderImpl::RejectNotUnderstood() {
  sig()->SendCommandReject(id_, RejectReason::kNotUnderstood, BufferView());
}

void SignalingChannel::ResponderImpl::RejectInvalidChannelId(ChannelId local_cid,
                                                             ChannelId remote_cid) {
  uint16_t ids[2];
  ids[0] = htole16(local_cid);
  ids[1] = htole16(remote_cid);
  sig()->SendCommandReject(id_, RejectReason::kInvalidCID, BufferView(ids, sizeof(ids)));
}

bool SignalingChannel::SendPacket(CommandCode code, uint8_t identifier, const ByteBuffer& data) {
  ZX_DEBUG_ASSERT(IsCreationThreadCurrent());
  return Send(BuildPacket(code, identifier, data));
}

bool SignalingChannel::Send(ByteBufferPtr packet) {
  ZX_DEBUG_ASSERT(IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(packet);
  ZX_DEBUG_ASSERT(packet->size() >= sizeof(CommandHeader));

  if (!is_open())
    return false;

  // While 0x00 is an illegal command identifier (see v5.0, Vol 3, Part A,
  // Section 4) we don't assert that here. When we receive a command that uses
  // 0 as the identifier, we reject the command and use that identifier in the
  // response rather than assert and crash.
  __UNUSED SignalingPacket reply(packet.get(), packet->size() - sizeof(CommandHeader));
  ZX_DEBUG_ASSERT(reply.header().code);
  ZX_DEBUG_ASSERT(reply.payload_size() == le16toh(reply.header().length));
  ZX_DEBUG_ASSERT(chan_);

  return chan_->Send(std::move(packet));
}

ByteBufferPtr SignalingChannel::BuildPacket(CommandCode code, uint8_t identifier,
                                            const ByteBuffer& data) {
  ZX_DEBUG_ASSERT(data.size() <= std::numeric_limits<uint16_t>::max());

  auto buffer = NewSlabBuffer(sizeof(CommandHeader) + data.size());
  ZX_ASSERT(buffer);

  MutableSignalingPacket packet(buffer.get(), data.size());
  packet.mutable_header()->code = code;
  packet.mutable_header()->id = identifier;
  packet.mutable_header()->length = htole16(static_cast<uint16_t>(data.size()));
  packet.mutable_payload_data().Write(data);

  return buffer;
}

bool SignalingChannel::SendCommandReject(uint8_t identifier, RejectReason reason,
                                         const ByteBuffer& data) {
  ZX_DEBUG_ASSERT(data.size() <= kCommandRejectMaxDataLength);

  constexpr size_t kMaxPayloadLength = sizeof(CommandRejectPayload) + kCommandRejectMaxDataLength;
  StaticByteBuffer<kMaxPayloadLength> rej_buf;

  MutablePacketView<CommandRejectPayload> reject(&rej_buf, data.size());
  reject.mutable_header()->reason = htole16(reason);
  reject.mutable_payload_data().Write(data);

  return SendPacket(kCommandRejectCode, identifier, reject.data());
}

CommandId SignalingChannel::GetNextCommandId() {
  // Recycling identifiers is permitted and only 0x00 is invalid (v5.0 Vol 3,
  // Part A, Section 4).
  const auto cmd = next_cmd_id_++;
  if (next_cmd_id_ == kInvalidCommandId) {
    next_cmd_id_ = 0x01;
  }

  return cmd;
}

void SignalingChannel::OnChannelClosed() {
  ZX_DEBUG_ASSERT(IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(is_open());

  is_open_ = false;
}

void SignalingChannel::OnRxBFrame(ByteBufferPtr sdu) {
  ZX_DEBUG_ASSERT(IsCreationThreadCurrent());

  if (!is_open())
    return;

  DecodeRxUnit(std::move(sdu), fit::bind_member(this, &SignalingChannel::CheckAndDispatchPacket));
}

void SignalingChannel::CheckAndDispatchPacket(const SignalingPacket& packet) {
  if (packet.size() > mtu()) {
    // Respond with our signaling MTU.
    uint16_t rsp_mtu = htole16(mtu());
    BufferView rej_data(&rsp_mtu, sizeof(rsp_mtu));
    SendCommandReject(packet.header().id, RejectReason::kSignalingMTUExceeded, rej_data);
  } else if (!packet.header().id) {
    // "Signaling identifier 0x00 is an illegal identifier and shall never be
    // used in any command" (v5.0, Vol 3, Part A, Section 4).
    bt_log(TRACE, "l2cap", "illegal signaling cmd ID: 0x00; reject");
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
  } else if (!HandlePacket(packet)) {
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
  }
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
