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
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {
pw::Status GattNotifyChannel::Write(pw::span<const uint8_t> attribute_value) {
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
  if (attribute_value.size() > max_attribute_size) {
    PW_LOG_ERROR("Attribute too large (%zu > %d). So will not process.",
                 attribute_value.size(),
                 max_attribute_size);
    return pw::Status::InvalidArgument();
  }

  size_t att_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + attribute_value.size();
  pw::Result<H4PacketWithH4> h4_result =
      PopulateTxL2capPacketDuringWrite(att_size);
  if (!h4_result.ok()) {
    // This can fail as a result of the L2CAP PDU not fitting in an H4 buffer
    // or if all buffers are occupied.
    // TODO: https://pwbug.dev/379337260 - Once we support ACL fragmentation,
    // this function will not fail due to the L2CAP PDU size not fitting.
    return h4_result.status();
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  // Write ATT PDU.
  PW_TRY_ASSIGN(
      auto acl,
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan()));
  PW_TRY_ASSIGN(auto l2cap,
                MakeEmbossWriter<emboss::BFrameWriter>(
                    acl.payload().BackingStorage().data(),
                    acl.payload().BackingStorage().SizeInBytes()));
  PW_TRY_ASSIGN(auto att_notify,
                MakeEmbossWriter<emboss::AttHandleValueNtfWriter>(
                    attribute_value.size(),
                    l2cap.payload().BackingStorage().data(),
                    att_size));
  att_notify.attribute_opcode().Write(emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  att_notify.attribute_handle().Write(attribute_handle_);

  PW_CHECK(
      TryToCopyToEmbossStruct(att_notify.attribute_value(), attribute_value));

  return QueuePacket(std::move(h4_packet));
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
  return GattNotifyChannel(/*l2cap_channel_manager=*/l2cap_channel_manager,
                           /*connection_handle=*/connection_handle,
                           /*attribute_handle=*/attribute_handle,
                           std::move(event_fn));
}

GattNotifyChannel::GattNotifyChannel(L2capChannelManager& l2cap_channel_manager,
                                     uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     ChannelEventCallback&& event_fn)
    : L2capChannel(/*l2cap_channel_manager=*/l2cap_channel_manager,
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
