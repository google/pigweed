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
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

pw::Result<BasicL2capChannel> BasicL2capChannel::Create(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_host_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
  if (!AreValidParameters(/*connection_handle=*/connection_handle,
                          /*local_cid=*/local_cid,
                          /*remote_cid=*/remote_cid)) {
    return pw::Status::InvalidArgument();
  }

  return BasicL2capChannel(
      /*l2cap_channel_manager=*/l2cap_channel_manager,
      /*connection_handle=*/connection_handle,
      /*transport=*/transport,
      /*local_cid=*/local_cid,
      /*remote_cid=*/remote_cid,
      /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
      /*payload_from_host_fn=*/std::move(payload_from_host_fn),
      /*event_fn=*/std::move(event_fn));
}

StatusWithMultiBuf BasicL2capChannel::Write(multibuf::MultiBuf&& payload) {
  if (!IsOkL2capDataLength(payload.size())) {
    PW_LOG_WARN("Payload (%zu bytes) is too large. So will not process.",
                payload.size());
    return {Status::InvalidArgument(), std::move(payload)};
  }

  return L2capChannel::Write(std::move(payload));
}

std::optional<H4PacketWithH4> BasicL2capChannel::GenerateNextTxPacket() {
  if (state() != State::kRunning || PayloadQueueEmpty()) {
    return std::nullopt;
  }

  ConstByteSpan payload = GetFrontPayloadSpan();

  pw::Result<H4PacketWithH4> result =
      PopulateTxL2capPacket(payload.size_bytes());
  if (!result.ok()) {
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(result.value());

  Result<emboss::AclDataFrameWriter> result2 =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK(result2.ok());
  emboss::AclDataFrameWriter acl = result2.value();

  // At this point we assume we can return a PDU with the payload.
  PopFrontPayload();

  emboss::BFrameWriter bframe = emboss::MakeBFrameView(
      acl.payload().BackingStorage().data(), acl.payload().SizeInBytes());
  PW_CHECK(bframe.IsComplete());

  PW_CHECK(TryToCopyToEmbossStruct(bframe.payload(), payload));

  PW_CHECK(acl.Ok());
  PW_CHECK(bframe.Ok());

  return h4_packet;
}

BasicL2capChannel::BasicL2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_host_fn,
    Function<void(L2capChannelEvent event)>&& event_fn)
    : L2capChannel(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*connection_handle=*/connection_handle,
          /*transport=*/transport,
          /*local_cid=*/local_cid,
          /*remote_cid=*/remote_cid,
          /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
          /*payload_from_host_fn=*/std::move(payload_from_host_fn),
          /*event_fn=*/std::move(event_fn)) {
  PW_LOG_INFO("btproxy: BasicL2capChannel ctor");
}

BasicL2capChannel::~BasicL2capChannel() {
  // Don't log dtor of moved-from channels.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: BasicL2capChannel dtor");
  }
}

bool BasicL2capChannel::DoHandlePduFromController(pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Received invalid B-frame. So will drop.",
                 local_cid());
    return true;
  }

  return SendPayloadFromControllerToClient(
      span(bframe_view->payload().BackingStorage().data(),
           bframe_view->payload().SizeInBytes()));
}

bool BasicL2capChannel::HandlePduFromHost(pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Host transmitted invalid B-frame. So will drop.",
                 local_cid());
    return true;
  }

  return SendPayloadFromHostToClient(
      span(bframe_view->payload().BackingStorage().data(),
           bframe_view->payload().SizeInBytes()));
}

}  // namespace pw::bluetooth::proxy
