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

#include "pw_bluetooth_proxy/basic_l2cap_channel.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

pw::Result<BasicL2capChannel> BasicL2capChannel::Create(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t local_cid,
    uint16_t remote_cid,
    pw::Function<void(pw::span<uint8_t> payload)>&&
        payload_from_controller_fn) {
  if (!L2capReadChannel::AreValidParameters(connection_handle, local_cid) ||
      !L2capWriteChannel::AreValidParameters(connection_handle, remote_cid)) {
    return pw::Status::InvalidArgument();
  }

  return BasicL2capChannel(
      /*l2cap_channel_manager=*/l2cap_channel_manager,
      /*connection_handle=*/connection_handle,
      /*local_cid=*/local_cid,
      /*remote_cid=*/remote_cid,
      /*payload_from_controller_fn=*/std::move(payload_from_controller_fn));
}

pw::Status BasicL2capChannel::Write(pw::span<const uint8_t> payload) {
  // TODO: https://pwbug.dev/360929142 - Reject payloads exceeding MTU.

  pw::Result<H4PacketWithH4> h4_result = PopulateTxL2capPacket(payload.size());
  if (!h4_result.ok()) {
    // This can fail as a result of the L2CAP PDU not fitting in an H4 buffer
    // or if all buffers are occupied.
    // TODO: https://pwbug.dev/365179076 - Once we support ACL fragmentation,
    // this function will not fail due to the L2CAP PDU size not fitting.
    return h4_result.status();
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  PW_TRY_ASSIGN(
      auto acl,
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan()));
  PW_TRY_ASSIGN(auto bframe,
                MakeEmbossWriter<emboss::BFrameWriter>(
                    acl.payload().BackingStorage().data(),
                    acl.payload().BackingStorage().SizeInBytes()));
  std::memcpy(
      bframe.payload().BackingStorage().data(), payload.data(), payload.size());

  return QueuePacket(std::move(h4_packet));
}

BasicL2capChannel::BasicL2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t local_cid,
    uint16_t remote_cid,
    pw::Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn)
    : L2capReadChannel(l2cap_channel_manager,
                       std::move(payload_from_controller_fn),
                       connection_handle,
                       local_cid),
      L2capWriteChannel(l2cap_channel_manager,
                        connection_handle,
                        AclTransportType::kLe,
                        remote_cid) {}

bool BasicL2capChannel::HandlePduFromController(pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Received invalid B-frame. So will drop.",
                 local_cid());
  } else {
    SendPayloadFromControllerToClient(
        span(bframe_view->payload().BackingStorage().data(),
             bframe_view->payload().SizeInBytes()));
  }
  return true;
}

bool BasicL2capChannel::HandlePduFromHost(pw::span<uint8_t>) {
  // Always forward to controller.
  return false;
}

}  // namespace pw::bluetooth::proxy
