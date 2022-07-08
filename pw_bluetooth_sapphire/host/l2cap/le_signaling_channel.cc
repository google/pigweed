// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "le_signaling_channel.h"

#include "channel.h"
#include "logical_link.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::l2cap::internal {

LESignalingChannel::LESignalingChannel(fxl::WeakPtr<Channel> chan, hci_spec::ConnectionRole role)
    : SignalingChannel(std::move(chan), role) {
  set_mtu(kMinLEMTU);
}

void LESignalingChannel::DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) {
  // "[O]nly one command per C-frame shall be sent over [the LE] Fixed Channel"
  // (v5.0, Vol 3, Part A, Section 4).
  ZX_DEBUG_ASSERT(sdu);
  if (sdu->size() < sizeof(CommandHeader)) {
    bt_log(DEBUG, "l2cap-le", "sig: dropped malformed LE signaling packet");
    return;
  }

  SignalingPacket packet(sdu.get());
  uint16_t expected_payload_length = le16toh(packet.header().length);
  if (expected_payload_length != sdu->size() - sizeof(CommandHeader)) {
    bt_log(DEBUG, "l2cap-le", "sig: packet size mismatch (expected: %u, recv: %zu); drop",
           expected_payload_length, sdu->size() - sizeof(CommandHeader));
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
    return;
  }

  cb(SignalingPacket(sdu.get(), expected_payload_length));
}

bool LESignalingChannel::IsSupportedResponse(CommandCode code) const {
  switch (code) {
    case kCommandRejectCode:
    case kConnectionParameterUpdateResponse:
    case kDisconnectionResponse:
    case kLECreditBasedConnectionResponse:
      return true;
  }

  // Other response-type commands are for AMP/BREDR and are not supported.
  return false;
}

}  // namespace bt::l2cap::internal
