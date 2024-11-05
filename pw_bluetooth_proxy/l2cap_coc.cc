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

#include "pw_bluetooth_proxy/l2cap_coc.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_write_channel.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

pw::Status L2capCoc::Stop() {
  if (state_ == CocState::kStopped) {
    return Status::InvalidArgument();
  }
  state_ = CocState::kStopped;
  return OkStatus();
}

pw::Status L2capCoc::Write(pw::span<const uint8_t> payload) {
  if (state_ == CocState::kStopped) {
    return Status::FailedPrecondition();
  }

  if (payload.size() > tx_mtu_) {
    PW_LOG_ERROR(
        "Payload (%zu bytes) exceeds MTU (%d bytes). So will not process.",
        payload.size(),
        tx_mtu_);
    return pw::Status::InvalidArgument();
  }
  // We do not currently support segmentation, so the payload is required to fit
  // within the remote peer's Maximum PDU payload Size.
  // TODO: https://pwbug.dev/360932103 - Support packet segmentation.
  if (payload.size() > tx_mps_) {
    PW_LOG_ERROR(
        "Payload (%zu bytes) exceeds MPS (%d bytes). So will not process.",
        payload.size(),
        tx_mps_);
    return pw::Status::InvalidArgument();
  }

  // 2 bytes for SDU length field
  size_t l2cap_data_length = payload.size() + 2;
  pw::Result<H4PacketWithH4> h4_result =
      PopulateTxL2capPacket(l2cap_data_length);
  if (!h4_result.ok()) {
    // This can fail as a result of the L2CAP PDU not fitting in an H4 buffer
    // or if all buffers are occupied.
    // TODO: https://pwbug.dev/365179076 - Once we support ACL fragmentation,
    // this function will not fail due to the L2CAP PDU size not fitting.
    return h4_result.status();
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  emboss::AclDataFrameWriter acl =
      MakeEmboss<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  // Write payload.
  emboss::FirstKFrameWriter kframe =
      emboss::MakeFirstKFrameView(acl.payload().BackingStorage().data(),
                                  acl.payload().BackingStorage().SizeInBytes());
  kframe.sdu_length().Write(payload.size());
  std::memcpy(
      kframe.payload().BackingStorage().data(), payload.data(), payload.size());

  // H4 packet is hereby moved. Either ACL data channel will move packet to
  // controller or will be unable to send packet. In either case, packet will be
  // destructed, so its release function will be invoked.
  return SendL2capPacket(std::move(h4_packet));
}

pw::Result<L2capCoc> L2capCoc::Create(
    AclDataChannel& acl_data_channel,
    H4Storage& h4_storage,
    uint16_t connection_handle,
    CocConfig rx_config,
    CocConfig tx_config,
    pw::Function<void(Event event)>&& event_fn) {
  if (!L2capWriteChannel::AreValidParameters(connection_handle,
                                             tx_config.cid)) {
    return pw::Status::InvalidArgument();
  }

  if (tx_config.mps < emboss::L2capLeCreditBasedConnectionReq::min_mps() ||
      tx_config.mps > emboss::L2capLeCreditBasedConnectionReq::max_mps()) {
    PW_LOG_ERROR(
        "Tx MPS (%d octets) invalid. L2CAP implementations shall support a "
        "minimum MPS of 23 octets and may support an MPS up to 65533 octets.",
        tx_config.mps);
    return pw::Status::InvalidArgument();
  }

  return L2capCoc(acl_data_channel,
                  h4_storage,
                  connection_handle,
                  rx_config,
                  tx_config,
                  std::move(event_fn));
}

L2capCoc::L2capCoc(AclDataChannel& acl_data_channel,
                   H4Storage& h4_storage,
                   uint16_t connection_handle,
                   [[maybe_unused]] CocConfig rx_config,
                   CocConfig tx_config,
                   pw::Function<void(Event event)>&& event_fn)
    : L2capWriteChannel(
          acl_data_channel, h4_storage, connection_handle, tx_config.cid),
      state_(CocState::kRunning),
      tx_mtu_(tx_config.mtu),
      tx_mps_(tx_config.mps),
      event_fn_(std::move(event_fn)) {}

}  // namespace pw::bluetooth::proxy
