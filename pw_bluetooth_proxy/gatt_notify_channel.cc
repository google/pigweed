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

#include "pw_bluetooth_proxy/gatt_notify_channel.h"

#include "pw_assert/check.h"
#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

std::optional<H4PacketWithH4> GattNotifyChannel::GenerateNextTxPacket() {
  if (state() != State::kRunning || PayloadQueueEmpty()) {
    return std::nullopt;
  }

  ConstByteSpan attribute_value = GetFrontPayloadSpan();

  std::optional<uint16_t> max_l2cap_payload_size = MaxL2capPayloadSize();
  // This should have been caught during Write.
  PW_CHECK(max_l2cap_payload_size);
  const uint16_t max_attribute_size =
      *max_l2cap_payload_size - emboss::AttHandleValueNtf::MinSizeInBytes();
  // This should have been caught during Write.
  PW_CHECK(attribute_value.size() < max_attribute_size);

  size_t att_frame_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + attribute_value.size();

  pw::Result<H4PacketWithH4> result = PopulateTxL2capPacket(att_frame_size);
  if (!result.ok()) {
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(result.value());

  // Write ATT PDU.
  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK_OK(acl);
  PW_CHECK(acl->Ok());

  Result<emboss::BFrameWriter> l2cap = MakeEmbossWriter<emboss::BFrameWriter>(
      acl->payload().BackingStorage().data(),
      acl->payload().BackingStorage().SizeInBytes());
  PW_CHECK_OK(l2cap);
  PW_CHECK(l2cap->Ok());

  PW_CHECK(att_frame_size == l2cap->payload().BackingStorage().SizeInBytes());
  Result<emboss::AttHandleValueNtfWriter> att_notify =
      MakeEmbossWriter<emboss::AttHandleValueNtfWriter>(
          attribute_value.size(),
          l2cap->payload().BackingStorage().data(),
          att_frame_size);
  PW_CHECK_OK(att_notify);

  att_notify->attribute_opcode().Write(emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  att_notify->attribute_handle().Write(attribute_handle_);
  PW_CHECK(
      TryToCopyToEmbossStruct(att_notify->attribute_value(), attribute_value));
  PW_CHECK(att_notify->Ok());

  // All content has been copied from the front payload, so release it.
  PopFrontPayload();

  return h4_packet;
}

Status GattNotifyChannel::DoCheckWriteParameter(
    pw::multibuf::MultiBuf& payload) {
  std::optional<uint16_t> max_l2cap_payload_size = MaxL2capPayloadSize();
  if (!max_l2cap_payload_size) {
    PW_LOG_ERROR("Tried to write before LE_Read_Buffer_Size processed.");
    return Status::FailedPrecondition();
  }
  if (*max_l2cap_payload_size <= emboss::AttHandleValueNtf::MinSizeInBytes()) {
    PW_LOG_ERROR("LE ACL data packet size limit does not support writing.");
    return Status::FailedPrecondition();
  }
  const uint16_t max_attribute_size =
      *max_l2cap_payload_size - emboss::AttHandleValueNtf::MinSizeInBytes();
  if (payload.size() > max_attribute_size) {
    PW_LOG_ERROR("Attribute too large (%zu > %d). So will not process.",
                 payload.size(),
                 max_attribute_size);
    return pw::Status::InvalidArgument();
  }

  return pw::OkStatus();
}

pw::Result<GattNotifyChannel> GattNotifyChannel::Create(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t attribute_handle,
    ChannelEventCallback&& event_fn) {
  if (!AreValidParameters(/*connection_handle=*/connection_handle,
                          /*local_cid=*/kAttributeProtocolCID,
                          /*remote_cid=*/kAttributeProtocolCID)) {
    return pw::Status::InvalidArgument();
  }
  if (attribute_handle == 0) {
    PW_LOG_ERROR("Attribute handle cannot be 0.");
    return pw::Status::InvalidArgument();
  }
  GattNotifyChannel channel(/*l2cap_channel_manager=*/l2cap_channel_manager,
                            /*connection_handle=*/connection_handle,
                            /*attribute_handle=*/attribute_handle,
                            std::move(event_fn));
  channel.Init();
  return channel;
}

GattNotifyChannel::GattNotifyChannel(L2capChannelManager& l2cap_channel_manager,
                                     uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     ChannelEventCallback&& event_fn)
    : SingleChannelProxy(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*rx_multibuf_allocator*/ nullptr,
          /*connection_handle=*/connection_handle,
          /*transport=*/AclTransportType::kLe,
          /*local_cid=*/kAttributeProtocolCID,
          /*remote_cid=*/kAttributeProtocolCID,
          /*payload_from_controller_fn=*/nullptr,
          /*payload_from_host_fn=*/nullptr,
          /*event_fn=*/std::move(event_fn)),
      attribute_handle_(attribute_handle) {
  PW_LOG_INFO("btproxy: GattNotifyChannel ctor - attribute_handle: %u",
              attribute_handle_);
}

GattNotifyChannel::~GattNotifyChannel() {
  PW_LOG_INFO("btproxy: GattNotifyChannel dtor - attribute_handle: %u",
              attribute_handle_);
}

}  // namespace pw::bluetooth::proxy
