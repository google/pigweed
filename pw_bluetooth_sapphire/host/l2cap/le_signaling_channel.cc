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

#include "pw_bluetooth_sapphire/internal/host/l2cap/le_signaling_channel.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"

namespace bt::l2cap::internal {

LESignalingChannel::LESignalingChannel(
    Channel::WeakPtr chan,
    pw::bluetooth::emboss::ConnectionRole role,
    pw::async::Dispatcher& dispatcher)
    : SignalingChannel(std::move(chan), role, dispatcher) {
  set_mtu(kMinLEMTU);
}

void LESignalingChannel::DecodeRxUnit(ByteBufferPtr sdu,
                                      const SignalingPacketHandler& cb) {
  // "[O]nly one command per C-frame shall be sent over [the LE] Fixed Channel"
  // (v5.0, Vol 3, Part A, Section 4).
  BT_DEBUG_ASSERT(sdu);
  if (sdu->size() < sizeof(CommandHeader)) {
    bt_log(DEBUG, "l2cap-le", "sig: dropped malformed LE signaling packet");
    return;
  }

  SignalingPacket packet(sdu.get());
  uint16_t expected_payload_length = le16toh(packet.header().length);
  if (expected_payload_length != sdu->size() - sizeof(CommandHeader)) {
    bt_log(DEBUG,
           "l2cap-le",
           "sig: packet size mismatch (expected: %u, recv: %zu); drop",
           expected_payload_length,
           sdu->size() - sizeof(CommandHeader));
    SendCommandReject(
        packet.header().id, RejectReason::kNotUnderstood, BufferView());
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
