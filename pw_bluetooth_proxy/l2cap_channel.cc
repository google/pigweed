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

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"

#include <mutex>

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

void L2capChannel::MoveFields(L2capChannel& other) {
  // TODO: https://pwbug.dev/380504851 - Add tests for move operators.
  connection_handle_ = other.connection_handle();
  transport_ = other.transport();
  local_cid_ = other.local_cid();
  remote_cid_ = other.remote_cid();

  payload_from_controller_fn_ = std::move(other.payload_from_controller_fn_);
  payload_from_host_fn_ = std::move(other.payload_from_host_fn_);
  rx_multibuf_allocator_ = other.rx_multibuf_allocator_;

  l2cap_channel_manager_.DeregisterChannel(other);
  l2cap_channel_manager_.RegisterChannel(*this);
}

L2capChannel::L2capChannel(L2capChannel&& other)
    : ClientChannel(static_cast<ClientChannel&&>(other)),
      l2cap_channel_manager_(other.l2cap_channel_manager_) {
  MoveFields(other);
}

L2capChannel& L2capChannel::operator=(L2capChannel&& other) {
  if (this != &other) {
    l2cap_channel_manager_.DeregisterChannel(*this);
    MoveFields(other);
  }
  return *this;
}

L2capChannel::~L2capChannel() {
  // Don't log dtor of moved-from channels.
  if (state() != State::kUndefined) {
    PW_LOG_INFO(
        "btproxy: L2capChannel dtor - transport_: %u, connection_handle_ : "
        "%#x, local_cid_: %#x, remote_cid_: %#x, state_: %u",
        cpp23::to_underlying(transport_),
        connection_handle_,
        local_cid_,
        remote_cid_,
        cpp23::to_underlying(state()));
  }

  // Channel objects may outlive `ProxyHost`, but they are closed on
  // `ProxyHost` dtor, so this check will prevent a crash from trying to access
  // a destructed `L2capChannelManager`.
  if (state() != State::kClosed) {
    l2cap_channel_manager_.DeregisterChannel(*this);
  }
}

void L2capChannel::HandleStop() {
  PW_LOG_INFO(
      "btproxy: L2capChannel::Stop - transport_: %u, connection_handle_: %#x, "
      "local_cid_: %#x, remote_cid_: %#x, previous state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle_,
      local_cid_,
      remote_cid_,
      cpp23::to_underlying(state()));
}

void L2capChannel::HandleClose() {
  l2cap_channel_manager_.DeregisterChannel(*this);
}

bool L2capChannel::HandlePduFromController(pw::span<uint8_t> l2cap_pdu) {
  if (state() != State::kRunning) {
    PW_LOG_ERROR(
        "btproxy: L2capChannel::OnPduReceivedFromController on non-running "
        "channel. local_cid: %#x, remote_cid: %#x, state: %u",
        local_cid(),
        remote_cid(),
        cpp23::to_underlying(state()));
    SendEvent(L2capChannelEvent::kRxWhileStopped);
    return true;
  }
  return DoHandlePduFromController(l2cap_pdu);
}

L2capChannel::L2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    multibuf::MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    Function<void(L2capChannelEvent event)>&& event_fn)
    : ClientChannel(std::move(event_fn)),
      l2cap_channel_manager_(l2cap_channel_manager),
      connection_handle_(connection_handle),
      transport_(transport),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      rx_multibuf_allocator_(rx_multibuf_allocator),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)),
      payload_from_host_fn_(std::move(payload_from_host_fn)) {
  PW_LOG_INFO(
      "btproxy: L2capChannel ctor - transport_: %u, connection_handle_ : %u, "
      "local_cid_ : %#x, remote_cid_: %#x",
      cpp23::to_underlying(transport_),
      connection_handle_,
      local_cid_,
      remote_cid_);

  l2cap_channel_manager_.RegisterChannel(*this);
}

bool L2capChannel::AreValidParameters(uint16_t connection_handle,
                                      uint16_t local_cid,
                                      uint16_t remote_cid) {
  if (connection_handle > kMaxValidConnectionHandle) {
    PW_LOG_ERROR(
        "Invalid connection handle %#x. Maximum connection handle is 0x0EFF.",
        connection_handle);
    return false;
  }
  if (local_cid == 0 || remote_cid == 0) {
    PW_LOG_ERROR("L2CAP channel identifier 0 is not valid.");
    return false;
  }
  return true;
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateTxL2capPacket(
    uint16_t data_length) {
  return PopulateL2capPacket(data_length);
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateTxL2capPacketDuringWrite(
    uint16_t data_length) {
  pw::Result<H4PacketWithH4> packet_result = PopulateL2capPacket(data_length);
  if (packet_result.status().IsUnavailable()) {
    // If there were no buffers, they are all in the queue currently. This can
    // happen if queue size == buffer count. Mark that a writer is getting an
    // Unavailable status, and should be notified when queue space opens up.
    SetNotifyOnDequeue();
  }
  return packet_result;
}

namespace {
constexpr size_t H4SizeForL2capData(uint16_t data_length) {
  const size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  const size_t acl_packet_size =
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes() + l2cap_packet_size;
  return sizeof(emboss::H4PacketType) + acl_packet_size;
}

}  // namespace

bool L2capChannel::IsOkL2capDataLength(uint16_t data_length) {
  return H4SizeForL2capData(data_length) <=
         l2cap_channel_manager_.GetH4BuffSize();
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateL2capPacket(
    uint16_t data_length) {
  const size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  const size_t h4_packet_size = H4SizeForL2capData(data_length);

  pw::Result<H4PacketWithH4> h4_packet_res =
      l2cap_channel_manager_.GetAclH4Packet(h4_packet_size);
  if (!h4_packet_res.ok()) {
    return h4_packet_res.status();
  }
  H4PacketWithH4 h4_packet = std::move(h4_packet_res.value());
  h4_packet.SetH4Type(emboss::H4PacketType::ACL_DATA);

  PW_TRY_ASSIGN(
      auto acl,
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan()));
  acl.header().handle().Write(connection_handle_);
  // TODO: https://pwbug.dev/360932103 - Support packet segmentation, so this
  // value will not always be FIRST_NON_FLUSHABLE.
  acl.header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
  acl.header().broadcast_flag().Write(
      emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
  acl.data_total_length().Write(l2cap_packet_size);

  PW_TRY_ASSIGN(auto l2cap_header,
                MakeEmbossWriter<emboss::BasicL2capHeaderWriter>(
                    acl.payload().BackingStorage().data(),
                    emboss::BasicL2capHeader::IntrinsicSizeInBytes()));
  l2cap_header.pdu_length().Write(data_length);
  l2cap_header.channel_id().Write(remote_cid_);

  return h4_packet;
}

std::optional<uint16_t> L2capChannel::MaxL2capPayloadSize() const {
  std::optional<uint16_t> le_acl_data_packet_length =
      l2cap_channel_manager_.le_acl_data_packet_length();
  if (!le_acl_data_packet_length) {
    return std::nullopt;
  }

  uint16_t max_acl_data_size_based_on_h4_buffer =
      l2cap_channel_manager_.GetH4BuffSize() - sizeof(emboss::H4PacketType) -
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes();
  uint16_t max_acl_data_size = std::min(max_acl_data_size_based_on_h4_buffer,
                                        *le_acl_data_packet_length);
  return max_acl_data_size - emboss::BasicL2capHeader::IntrinsicSizeInBytes();
}

void L2capChannel::HandlePacketsMayBeReadyToSend() {
  l2cap_channel_manager_.DrainChannelQueues();
}

//-------
//  Rx (protected)
//-------

bool L2capChannel::SendPayloadFromHostToClient(pw::span<uint8_t> payload) {
  return SendPayloadToClient(payload, payload_from_host_fn_);
}

bool L2capChannel::SendPayloadFromControllerToClient(
    pw::span<uint8_t> payload) {
  return SendPayloadToClient(payload, payload_from_controller_fn_);
}

bool L2capChannel::SendPayloadToClient(
    pw::span<uint8_t> payload, OptionalPayloadReceiveCallback& callback) {
  if (!callback) {
    return false;
  }

  std::optional<multibuf::MultiBuf> buffer =
      rx_multibuf_allocator()->AllocateContiguous(payload.size());

  if (!buffer) {
    PW_LOG_ERROR(
        "(CID %#x) Rx MultiBuf allocator out of memory. So stopping "
        "channel "
        "and reporting it needs to be closed.",
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxOutOfMemory);
    return true;
  }

  StatusWithSize status = buffer->CopyFrom(/*source=*/as_bytes(payload),
                                           /*position=*/0);
  PW_CHECK_OK(status);

  std::optional<multibuf::MultiBuf> client_multibuf =
      callback(std::move(*buffer));
  // If client returned multibuf to us, we drop it and indicate to caller that
  // packet should be forwarded. In the future when whole path is operating
  // with multibuf's, we could pass it back up to container to be forwarded.
  return !client_multibuf.has_value();
}

}  // namespace pw::bluetooth::proxy
