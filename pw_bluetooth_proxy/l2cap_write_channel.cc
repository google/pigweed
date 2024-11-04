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

#include "pw_bluetooth_proxy/internal/l2cap_write_channel.h"

#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/emboss_util.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

L2capWriteChannel::L2capWriteChannel(AclDataChannel& acl_data_channel,
                                     H4Storage& h4_storage,
                                     uint16_t connection_handle,
                                     uint16_t remote_cid)
    : connection_handle_(connection_handle),
      remote_cid_(remote_cid),
      h4_storage_(h4_storage),
      acl_data_channel_(acl_data_channel) {}

bool L2capWriteChannel::AreValidParameters(uint16_t connection_handle,
                                           uint16_t remote_cid) {
  if (connection_handle > kMaxValidConnectionHandle) {
    PW_LOG_ERROR(
        "Invalid connection handle 0x%X. Maximum connection handle is 0x0EFF.",
        connection_handle);
    return false;
  }
  if (remote_cid == 0) {
    PW_LOG_ERROR("L2CAP channel identifier 0 is not valid.");
    return false;
  }
  return true;
}

pw::Result<H4PacketWithH4> L2capWriteChannel::PopulateTxL2capPacket(
    uint16_t data_length) {
  size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  size_t acl_packet_size =
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes() + l2cap_packet_size;
  size_t h4_packet_size = sizeof(emboss::H4PacketType) + acl_packet_size;

  if (h4_packet_size > h4_storage_.GetH4BuffSize()) {
    PW_LOG_ERROR(
        "Requested packet is too large for H4 buffer. So will not send.");
    return pw::Status::InvalidArgument();
  }

  std::optional<span<uint8_t>> h4_buff = h4_storage_.ReserveH4Buff();
  if (!h4_buff) {
    PW_LOG_WARN("No H4 buffers available. So will not send.");
    return pw::Status::Unavailable();
  }

  H4PacketWithH4 h4_packet(
      span(h4_buff->data(), h4_packet_size),
      /*release_fn=*/[h4_storage = &h4_storage_](const uint8_t* buffer) {
        h4_storage->ReleaseH4Buff(buffer);
      });
  h4_packet.SetH4Type(emboss::H4PacketType::ACL_DATA);

  emboss::AclDataFrameWriter acl =
      MakeEmboss<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  acl.header().handle().Write(connection_handle_);
  // TODO: https://pwbug.dev/360932103 - Support packet segmentation, so this
  // value will not always be FIRST_NON_FLUSHABLE.
  acl.header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
  acl.header().broadcast_flag().Write(
      emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
  acl.data_total_length().Write(l2cap_packet_size);

  emboss::BasicL2capHeaderWriter l2cap_header =
      emboss::MakeBasicL2capHeaderView(
          acl.payload().BackingStorage().data(),
          emboss::BasicL2capHeader::IntrinsicSizeInBytes());
  l2cap_header.pdu_length().Write(data_length);
  l2cap_header.channel_id().Write(remote_cid_);

  return h4_packet;
}

pw::Status L2capWriteChannel::SendL2capPacket(H4PacketWithH4&& tx_packet) {
  if (!acl_data_channel_.SendAcl(std::move(tx_packet))) {
    return pw::Status::Unavailable();
  }
  return pw::OkStatus();
}

}  // namespace pw::bluetooth::proxy
