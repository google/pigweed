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

#include <cstdint>
#include <mutex>

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_multibuf/multibuf.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {

L2capCoc::L2capCoc(L2capCoc&& other)
    : L2capChannel(static_cast<L2capCoc&&>(other)),
      rx_multibuf_allocator_(other.rx_multibuf_allocator_),
      signaling_channel_(other.signaling_channel_),
      rx_mtu_(other.rx_mtu_),
      rx_mps_(other.rx_mps_),
      tx_mtu_(other.tx_mtu_),
      tx_mps_(other.tx_mps_),
      payload_from_controller_fn_(std::move(other.payload_from_controller_fn_)),
      receive_fn_multibuf_(std::move(other.receive_fn_multibuf_)) {
  std::lock_guard lock(mutex_);
  std::lock_guard other_lock(other.mutex_);
  tx_credits_ = other.tx_credits_;
  remaining_sdu_bytes_to_ignore_ = other.remaining_sdu_bytes_to_ignore_;
}

pw::Status L2capCoc::Write(pw::span<const uint8_t> payload) {
  if (state() != State::kRunning) {
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
    // TODO: https://pwbug.dev/379337260 - Once we support ACL fragmentation,
    // this function will not fail due to the L2CAP PDU size not fitting.
    return h4_result.status();
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  PW_TRY_ASSIGN(
      auto acl,
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan()));
  // Write payload.
  PW_TRY_ASSIGN(auto kframe,
                MakeEmbossWriter<emboss::FirstKFrameWriter>(
                    acl.payload().BackingStorage().data(),
                    acl.payload().BackingStorage().SizeInBytes()));
  kframe.sdu_length().Write(payload.size());
  if (!payload.empty()) {
    std::memcpy(kframe.payload().BackingStorage().data(),
                payload.data(),
                payload.size());
  }

  return QueuePacket(std::move(h4_packet));
}

namespace {

pw::span<const uint8_t> AsConstUint8Span(ConstByteSpan s) {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size_bytes()};
}

}  // namespace

StatusWithMultiBuf L2capCoc::Write(pw::multibuf::MultiBuf&& payload) {
  if (!payload.IsContiguous()) {
    return StatusWithMultiBuf{.status = Status::InvalidArgument()};
  }

  std::optional<ByteSpan> payload_span = payload.ContiguousSpan();
  if (!payload_span) {
    return StatusWithMultiBuf{.status = Status::InvalidArgument()};
  }

  Status status = Write(AsConstUint8Span(*payload_span));
  if (!status.ok()) {
    return StatusWithMultiBuf{.status = status, .buf = {std::move(payload)}};
  }

  return StatusWithMultiBuf{.status = pw::OkStatus()};
}

pw::Result<L2capCoc> L2capCoc::Create(
    pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
    L2capChannelManager& l2cap_channel_manager,
    L2capSignalingChannel* signaling_channel,
    uint16_t connection_handle,
    CocConfig rx_config,
    CocConfig tx_config,
    Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn,
    Function<void(multibuf::MultiBuf&& payload)>&& receive_fn_multibuf) {
  PW_CHECK(!receive_fn_multibuf || !payload_from_controller_fn);

  if (!AreValidParameters(/*connection_handle=*/connection_handle,
                          /*local_cid=*/rx_config.cid,
                          /*remote_cid=*/tx_config.cid)) {
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

  return L2capCoc(
      /*rx_multibuf_allocator=*/rx_multibuf_allocator,
      /*l2cap_channel_manager=*/l2cap_channel_manager,
      /*signaling_channel=*/signaling_channel,
      /*connection_handle=*/connection_handle,
      /*rx_config=*/rx_config,
      /*tx_config=*/tx_config,
      /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
      /*event_fn=*/std::move(event_fn),
      /*receive_fn_multibuf=*/std::move(receive_fn_multibuf));
}

pw::Status L2capCoc::SendAdditionalRxCredits(uint16_t additional_rx_credits) {
  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }
  PW_CHECK(signaling_channel_);
  return signaling_channel_->SendFlowControlCreditInd(local_cid(),
                                                      additional_rx_credits);
}

void L2capCoc::ProcessPduFromControllerMultibuf(span<uint8_t> kframe) {
  std::lock_guard lock(mutex_);
  ConstByteSpan kframe_payload;
  if (rx_sdu_bytes_remaining_ > 0) {
    // Received PDU that is part of current SDU being assembled.
    Result<emboss::SubsequentKFrameView> subsequent_kframe_view =
        MakeEmbossView<emboss::SubsequentKFrameView>(kframe);
    // Lower layers should not (and cannot) invoke this callback on a packet
    // with an incomplete basic L2CAP header.
    PW_CHECK_OK(subsequent_kframe_view);
    kframe_payload =
        as_bytes(span(subsequent_kframe_view->payload().BackingStorage().data(),
                      subsequent_kframe_view->payload_size().Read()));
  } else {
    // Received first (or only) PDU of SDU.
    Result<emboss::FirstKFrameView> first_kframe_view =
        MakeEmbossView<emboss::FirstKFrameView>(kframe);
    if (!first_kframe_view.ok()) {
      PW_LOG_ERROR(
          "(CID 0x%X) Buffer is too small for first K-frame. So stopping "
          "channel and reporting it needs to be closed.",
          local_cid());
      StopAndSendEvent(L2capChannelEvent::kRxInvalid);
      return;
    }

    rx_sdu_bytes_remaining_ = first_kframe_view->sdu_length().Read();
    rx_sdu_ =
        rx_multibuf_allocator_.AllocateContiguous(rx_sdu_bytes_remaining_);
    if (!rx_sdu_) {
      PW_LOG_ERROR(
          "(CID 0x%X) Rx MultiBuf allocator out of memory. So stopping channel "
          "and reporting it needs to be closed.",
          local_cid());
      StopAndSendEvent(L2capChannelEvent::kRxOutOfMemory);
      return;
    }

    kframe_payload =
        as_bytes(span(first_kframe_view->payload().BackingStorage().data(),
                      first_kframe_view->payload_size().Read()));
  }

  // Copy segment into rx_sdu_.
  StatusWithSize status = rx_sdu_->CopyFrom(/*source=*/kframe_payload,
                                            /*position=*/rx_sdu_offset_);
  if (status.IsResourceExhausted()) {
    // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the sum of the payload sizes
    // for the K-frames exceeds the specified SDU length, the receiver shall
    // disconnect the channel."
    PW_LOG_ERROR(
        "(CID 0x%X) Sum of K-frame payload sizes exceeds the specified SDU "
        "length. So stopping channel and reporting it needs to be closed.",
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxInvalid);
    return;
  }
  PW_CHECK_OK(status);

  rx_sdu_bytes_remaining_ -= kframe_payload.size();
  rx_sdu_offset_ += kframe_payload.size();

  if (rx_sdu_bytes_remaining_ == 0) {
    // We have a full SDU, so invoke client callback.
    receive_fn_multibuf_(std::move(*rx_sdu_));
    rx_sdu_ = std::nullopt;
    rx_sdu_offset_ = 0;
  }
}

bool L2capCoc::HandlePduFromController(pw::span<uint8_t> kframe) {
  // TODO: https://pwbug.dev/360934030 - Track rx_credits.
  if (state() != State::kRunning) {
    StopAndSendEvent(L2capChannelEvent::kRxWhileStopped);
    return true;
  }

  // TODO: https://pwbug.dev/369849508 - Make this the only path once clients
  // move to MultiBuf Write() API.
  if (receive_fn_multibuf_) {
    ProcessPduFromControllerMultibuf(kframe);
    return true;
  }

  std::lock_guard lock(mutex_);
  // If `remaining_sdu_bytes_to_ignore_` is nonzero, we are in state where we
  // are dropping continuing PDUs in a segmented SDU.
  if (remaining_sdu_bytes_to_ignore_ > 0) {
    Result<emboss::SubsequentKFrameView> subsequent_kframe_view =
        MakeEmbossView<emboss::SubsequentKFrameView>(kframe);
    if (!subsequent_kframe_view.ok()) {
      PW_LOG_ERROR(
          "(CID 0x%X) Buffer is too small for subsequent L2CAP K-frame. So "
          "will drop.",
          local_cid());
      return true;
    }
    PW_LOG_INFO(
        "(CID 0x%X) Dropping PDU that is part of current segmented SDU.",
        local_cid());
    if (subsequent_kframe_view->payload_size().Read() >
        remaining_sdu_bytes_to_ignore_) {
      // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the sum of the payload sizes
      // for the K-frames exceeds the specified SDU length, the receiver shall
      // disconnect the channel."
      PW_LOG_ERROR(
          "(CID 0x%X) Sum of K-frame payload sizes exceeds the specified SDU "
          "length. So stopping channel & reporting it needs to be closed.",
          local_cid());
      StopAndSendEvent(L2capChannelEvent::kRxInvalid);
    } else {
      remaining_sdu_bytes_to_ignore_ -=
          subsequent_kframe_view->payload_size().Read();
    }
    return true;
  }

  Result<emboss::FirstKFrameView> kframe_view =
      MakeEmbossView<emboss::FirstKFrameView>(kframe);
  if (!kframe_view.ok()) {
    PW_LOG_ERROR(
        "(CID 0x%X) Buffer is too small for L2CAP K-frame. So stopping channel "
        "& reporting it needs to be closed.",
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxInvalid);
    return true;
  }
  uint16_t sdu_length = kframe_view->sdu_length().Read();
  uint16_t payload_size = kframe_view->payload_size().Read();

  // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the SDU length field value exceeds
  // the receiver's MTU, the receiver shall disconnect the channel."
  if (sdu_length > rx_mtu_) {
    PW_LOG_ERROR(
        "(CID 0x%X) Rx K-frame SDU exceeds MTU. So stopping channel & "
        "reporting it needs to be closed.",
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxInvalid);
    return true;
  }

  // TODO: https://pwbug.dev/360932103 - Support SDU de-segmentation.
  // We don't support SDU de-segmentation yet. If we see a SDU size larger than
  // the current PDU size, we ignore that first PDU and all remaining PDUs for
  // that SDU (which we track via remaining bytes expected for the SDU).
  if (sdu_length > payload_size) {
    PW_LOG_ERROR(
        "(CID 0x%X) Encountered segmented L2CAP SDU (which is not yet "
        "supported). So will drop all PDUs in SDU.",
        local_cid());
    remaining_sdu_bytes_to_ignore_ = sdu_length - payload_size;
    return true;
  }

  // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the payload size of any K-frame
  // exceeds the receiver's MPS, the receiver shall disconnect the channel."
  if (payload_size > rx_mps_) {
    PW_LOG_ERROR(
        "(CID 0x%X) Rx K-frame payload exceeds MPU. So stopping channel & "
        "reporting it needs to be closed.",
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxInvalid);
    return true;
  }

  SendPayloadFromControllerToClient(pw::span(
      const_cast<uint8_t*>(kframe_view->payload().BackingStorage().data()),
      kframe_view->payload_size().Read()));
  return true;
}

bool L2capCoc::HandlePduFromHost(pw::span<uint8_t>) PW_LOCKS_EXCLUDED(mutex_) {
  // Always forward data from host to controller
  return false;
}

L2capCoc::L2capCoc(
    pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
    L2capChannelManager& l2cap_channel_manager,
    L2capSignalingChannel* signaling_channel,
    uint16_t connection_handle,
    CocConfig rx_config,
    CocConfig tx_config,
    Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn,
    Function<void(multibuf::MultiBuf&& payload)>&& receive_fn_multibuf)
    : L2capChannel(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*connection_handle=*/connection_handle,
          /*transport=*/AclTransportType::kLe,
          /*local_cid=*/rx_config.cid,
          /*remote_cid=*/tx_config.cid,
          /*payload_from_controller_fn=*/nullptr,
          /*event_fn=*/std::move(event_fn)),
      rx_multibuf_allocator_(rx_multibuf_allocator),
      signaling_channel_(signaling_channel),
      rx_mtu_(rx_config.mtu),
      rx_mps_(rx_config.mps),
      tx_mtu_(tx_config.mtu),
      tx_mps_(tx_config.mps),
      tx_credits_(tx_config.credits),
      remaining_sdu_bytes_to_ignore_(0),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)),
      receive_fn_multibuf_(std::move(receive_fn_multibuf)) {}

std::optional<H4PacketWithH4> L2capCoc::DequeuePacket() {
  if (state() != State::kRunning) {
    return std::nullopt;
  }

  std::lock_guard lock(mutex_);
  if (tx_credits_ == 0) {
    return std::nullopt;
  }

  std::optional<H4PacketWithH4> maybe_packet = L2capChannel::DequeuePacket();
  if (maybe_packet.has_value()) {
    --tx_credits_;
  }
  return maybe_packet;
}

void L2capCoc::AddCredits(uint16_t credits) {
  if (state() != State::kRunning) {
    PW_LOG_ERROR(
        "(CID 0x%X) Received credits on stopped CoC. So will ignore signal.",
        local_cid());
    return;
  }

  bool credits_previously_zero;
  {
    std::lock_guard lock(mutex_);

    // Core Spec v6.0 Vol 3, Part A, 10.1: "The device receiving the credit
    // packet shall disconnect the L2CAP channel if the credit count exceeds
    // 65535."
    if (credits > emboss::L2capLeCreditBasedConnectionReq::max_credit_value() -
                      tx_credits_) {
      StopAndSendEvent(L2capChannelEvent::kRxInvalid);
      return;
    }

    credits_previously_zero = tx_credits_ == 0;
    tx_credits_ += credits;
  }
  if (credits_previously_zero) {
    ReportPacketsMayBeReadyToSend();
  }
}

}  // namespace pw::bluetooth::proxy
