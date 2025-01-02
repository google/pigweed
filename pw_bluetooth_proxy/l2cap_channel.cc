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
  // TODO: https://pwbug.dev/380504851 - Add tests for move operators, including
  // confirmation that event is not sent on Close().
  state_ = other.state();
  connection_handle_ = other.connection_handle();
  transport_ = other.transport();
  local_cid_ = other.local_cid();
  remote_cid_ = other.remote_cid();
  event_fn_ = std::move(other.event_fn_);
  payload_from_controller_fn_ = std::move(other.payload_from_controller_fn_);
  {
    std::lock_guard lock(send_queue_mutex_);
    std::lock_guard other_lock(other.send_queue_mutex_);
    send_queue_ = std::move(other.send_queue_);
    notify_on_dequeue_ = other.notify_on_dequeue_;
    l2cap_channel_manager_.ReleaseChannel(other);
    l2cap_channel_manager_.RegisterChannel(*this);
  }
  other.Undefine();
}

L2capChannel::L2capChannel(L2capChannel&& other)
    : l2cap_channel_manager_(other.l2cap_channel_manager_) {
  MoveFields(other);
}

L2capChannel& L2capChannel::operator=(L2capChannel&& other) {
  if (this != &other) {
    l2cap_channel_manager_.ReleaseChannel(*this);
    MoveFields(other);
  }
  return *this;
}

L2capChannel::~L2capChannel() {
  // Don't log dtor of moved-from channels.
  if (state_ != State::kUndefined) {
    PW_LOG_INFO(
        "btproxy: L2capChannel dtor - transport_: %u, connection_handle_ : %u, "
        "local_cid_: %u, remote_cid_: %u, state_: %u",
        cpp23::to_underlying(transport_),
        connection_handle_,
        local_cid_,
        remote_cid_,
        cpp23::to_underlying(state_));
  }

  l2cap_channel_manager_.ReleaseChannel(*this);
  ClearQueue();
}

void L2capChannel::Stop() {
  PW_LOG_INFO(
      "btproxy: L2capChannel::Stop - transport_: %u, connection_handle_: %u, "
      "local_cid_: %u, remote_cid_: %u, previous state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle_,
      local_cid_,
      remote_cid_,
      cpp23::to_underlying(state_));

  PW_CHECK(state_ != State::kUndefined && state_ != State::kClosed);

  state_ = State::kStopped;
  ClearQueue();
}

void L2capChannel::Close() {
  PW_LOG_INFO(
      "btproxy: L2capChannel::Close - transport_: %u, "
      "connection_handle_: %u, local_cid_: %u, remote_cid_: %u, previous "
      "state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle_,
      local_cid_,
      remote_cid_,
      cpp23::to_underlying(state_));

  PW_CHECK(state_ != State::kUndefined);

  // Channel can be closed twice: once for an L2CAP disconnection, then again
  // for an HCI disconnection if client has not yet dtored channel object.
  if (state_ == State::kClosed) {
    return;
  }

  state_ = State::kClosed;
  ClearQueue();
  SendEvent(L2capChannelEvent::kChannelClosedByOther);
}

void L2capChannel::Undefine() { state_ = State::kUndefined; }

Status L2capChannel::QueuePacket(H4PacketWithH4&& packet) {
  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }

  Status status;
  {
    std::lock_guard lock(send_queue_mutex_);
    if (send_queue_.full()) {
      status = Status::Unavailable();
      notify_on_dequeue_ = true;
    } else {
      send_queue_.push(std::move(packet));
      status = OkStatus();
    }
  }
  ReportPacketsMayBeReadyToSend();
  return status;
}

std::optional<H4PacketWithH4> L2capChannel::DequeuePacket() {
  std::optional<H4PacketWithH4> packet;
  bool should_notify = false;
  {
    std::lock_guard lock(send_queue_mutex_);
    packet = GenerateNextTxPacket();
    if (packet) {
      should_notify = notify_on_dequeue_;
      notify_on_dequeue_ = false;
    }
  }

  if (should_notify) {
    SendEvent(L2capChannelEvent::kWriteAvailable);
  }

  return packet;
}

StatusWithMultiBuf L2capChannel::QueuePayload(multibuf::MultiBuf&& buf) {
  PW_CHECK(state() == State::kRunning);
  PW_CHECK(buf.IsContiguous());

  {
    std::lock_guard lock(send_queue_mutex_);
    if (payload_queue_.full()) {
      notify_on_dequeue_ = true;
      return {Status::Unavailable(), std::move(buf)};
    }
    payload_queue_.push(std::move(buf));
  }

  ReportPacketsMayBeReadyToSend();
  return {OkStatus(), std::nullopt};
}

void L2capChannel::PopFrontPayload() {
  PW_CHECK(!payload_queue_.empty());
  payload_queue_.pop();
}

ConstByteSpan L2capChannel::GetFrontPayloadSpan() const {
  PW_CHECK(!payload_queue_.empty());
  const multibuf::MultiBuf& buf = payload_queue_.front();
  std::optional<ConstByteSpan> span = buf.ContiguousSpan();
  PW_CHECK(span);
  return *span;
}

bool L2capChannel::PayloadQueueEmpty() const { return payload_queue_.empty(); }

bool L2capChannel::OnPduReceivedFromController(pw::span<uint8_t> l2cap_pdu) {
  if (state() != State::kRunning) {
    SendEvent(L2capChannelEvent::kRxWhileStopped);
    return true;
  }
  return HandlePduFromController(l2cap_pdu);
}

void L2capChannel::OnFragmentedPduReceived() {
  if (state() != State::kRunning) {
    SendEvent(L2capChannelEvent::kRxWhileStopped);
    return;
  }
  PW_LOG_ERROR(
      "(CID 0x%X) Fragmented L2CAP frame received, which is not yet supported. "
      "Channel is now stopped.",
      local_cid());
  SendEvent(L2capChannelEvent::kRxFragmented);
  Stop();
}

L2capChannel::L2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn)
    : l2cap_channel_manager_(l2cap_channel_manager),
      state_(State::kRunning),
      connection_handle_(connection_handle),
      transport_(transport),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      event_fn_(std::move(event_fn)),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)) {
  PW_LOG_INFO(
      "btproxy: L2capChannel ctor - transport_: %u, connection_handle_ : %u, "
      "local_cid_ : %u, remote_cid_: %u",
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
        "Invalid connection handle 0x%X. Maximum connection handle is 0x0EFF.",
        connection_handle);
    return false;
  }
  if (local_cid == 0 || remote_cid == 0) {
    PW_LOG_ERROR("L2CAP channel identifier 0 is not valid.");
    return false;
  }
  return true;
}

std::optional<H4PacketWithH4> L2capChannel::GenerateNextTxPacket() {
  if (send_queue_.empty()) {
    return std::nullopt;
  }
  H4PacketWithH4 packet = std::move(send_queue_.front());
  send_queue_.pop();
  return packet;
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateTxL2capPacket(
    uint16_t data_length) {
  return PopulateL2capPacket(data_length);
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateTxL2capPacketDuringWrite(
    uint16_t data_length) {
  pw::Result<H4PacketWithH4> packet_result = PopulateL2capPacket(data_length);
  if (packet_result.status().IsUnavailable()) {
    std::lock_guard lock(send_queue_mutex_);
    // If there were no buffers, they are all in the queue currently. This can
    // happen if queue size == buffer count. Mark that a writer is getting an
    // Unavailable status, and should be notified when queue space opens up.
    notify_on_dequeue_ = true;
  }
  return packet_result;
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateL2capPacket(
    uint16_t data_length) {
  const size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  const size_t acl_packet_size =
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes() + l2cap_packet_size;
  const size_t h4_packet_size = sizeof(emboss::H4PacketType) + acl_packet_size;

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

void L2capChannel::ReportPacketsMayBeReadyToSend() {
  l2cap_channel_manager_.DrainChannelQueues();
}

void L2capChannel::ClearQueue() {
  std::lock_guard lock(send_queue_mutex_);
  send_queue_.clear();
}

}  // namespace pw::bluetooth::proxy
