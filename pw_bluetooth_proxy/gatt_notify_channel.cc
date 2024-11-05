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

#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

pw::Status GattNotifyChannel::Write(pw::span<const uint8_t> attribute_value) {
  constexpr uint16_t kMaxAttributeSize =
      H4Storage::GetH4BuffSize() - sizeof(emboss::H4PacketType) -
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes() -
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() -
      emboss::AttHandleValueNtf::MinSizeInBytes();
  if (attribute_value.size() > kMaxAttributeSize) {
    PW_LOG_ERROR("Attribute too large (%zu > %d). So will not process.",
                 attribute_value.size(),
                 kMaxAttributeSize);
    return pw::Status::InvalidArgument();
  }

  size_t att_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + attribute_value.size();
  pw::Result<H4PacketWithH4> h4_result = PopulateTxL2capPacket(att_size);
  if (!h4_result.ok()) {
    // This can fail as a result of the L2CAP PDU not fitting in an H4 buffer
    // or if all buffers are occupied.
    // TODO: https://pwbug.dev/365179076 - Once we support ACL fragmentation,
    // this function will not fail due to the L2CAP PDU size not fitting.
    return h4_result.status();
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  // Write ATT PDU.
  emboss::AclDataFrameWriter acl =
      MakeEmboss<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  emboss::BFrameWriter l2cap =
      emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                             acl.payload().BackingStorage().SizeInBytes());
  emboss::AttHandleValueNtfWriter att_notify =
      emboss::MakeAttHandleValueNtfView(attribute_value.size(),
                                        l2cap.payload().BackingStorage().data(),
                                        att_size);
  att_notify.attribute_opcode().Write(emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  att_notify.attribute_handle().Write(attribute_handle_);
  std::memcpy(att_notify.attribute_value().BackingStorage().data(),
              attribute_value.data(),
              attribute_value.size());

  // H4 packet is hereby moved. Either ACL data channel will move packet to
  // controller or will be unable to send packet. In either case, packet will be
  // destructed, so its release function will be invoked.
  return SendL2capPacket(std::move(h4_packet));
}

pw::Result<GattNotifyChannel> GattNotifyChannel::Create(
    AclDataChannel& acl_data_channel,
    H4Storage& h4_storage,
    uint16_t connection_handle,
    uint16_t attribute_handle) {
  if (!L2capWriteChannel::AreValidParameters(connection_handle,
                                             kAttributeProtocolCID)) {
    return pw::Status::InvalidArgument();
  }
  if (attribute_handle == 0) {
    PW_LOG_ERROR("Attribute handle cannot be 0.");
    return pw::Status::InvalidArgument();
  }
  return GattNotifyChannel(
      acl_data_channel, h4_storage, connection_handle, attribute_handle);
}

GattNotifyChannel::GattNotifyChannel(AclDataChannel& acl_data_channel,
                                     H4Storage& h4_storage,
                                     uint16_t connection_handle,
                                     uint16_t attribute_handle)
    : L2capWriteChannel(acl_data_channel,
                        h4_storage,
                        connection_handle,
                        kAttributeProtocolCID),
      attribute_handle_(attribute_handle) {}

}  // namespace pw::bluetooth::proxy
