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

#include <cmath>
#include <cstdint>
#include <mutex>

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_multibuf/multibuf.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

namespace {

// TODO: b/353734827 - Allow client to determine this constant.
const float kRxCreditReplenishThreshold = 0.30;

}  // namespace

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
  {
    std::lock_guard lock(tx_mutex_);
    std::lock_guard other_lock(other.tx_mutex_);
    tx_credits_ = other.tx_credits_;
  }
  {
    std::lock_guard lock(rx_mutex_);
    std::lock_guard other_lock(other.rx_mutex_);
    remaining_sdu_bytes_to_ignore_ = other.remaining_sdu_bytes_to_ignore_;
    rx_sdu_ = std::move(other.rx_sdu_);
    rx_sdu_offset_ = other.rx_sdu_offset_;
    rx_sdu_bytes_remaining_ = other.rx_sdu_bytes_remaining_;
    rx_remaining_credits_ = other.rx_remaining_credits_;
    rx_total_credits_ = other.rx_total_credits_;
  }
}

StatusWithMultiBuf L2capCoc::Write(multibuf::MultiBuf&& payload) {
  if (payload.size() > tx_mtu_) {
    PW_LOG_ERROR(
        "Payload (%zu bytes) exceeds MTU (%d bytes). So will not process.",
        payload.size(),
        tx_mtu_);
    return {Status::InvalidArgument(), std::move(payload)};
  }

  return L2capChannel::Write(std::move(payload));
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

pw::Status L2capCoc::ReplenishRxCredits(uint16_t additional_rx_credits) {
  PW_CHECK(signaling_channel_);
  // SendFlowControlCreditInd logs if status is not ok, so no need to log here.
  return signaling_channel_->SendFlowControlCreditInd(
      local_cid(), additional_rx_credits, rx_multibuf_allocator_);
}

pw::Status L2capCoc::SendAdditionalRxCredits(uint16_t additional_rx_credits) {
  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }
  std::lock_guard lock(rx_mutex_);
  PW_CHECK(signaling_channel_);
  // SendFlowControlCreditInd logs if status is not ok, so no need to log here.
  Status status = ReplenishRxCredits(additional_rx_credits);
  if (status.ok()) {
    // We treat additional bumps from the client as bumping the total allowed
    // credits.
    rx_total_credits_ += additional_rx_credits;
    rx_remaining_credits_ += additional_rx_credits;
    PW_LOG_INFO(
        "btproxy: L2capCoc::SendAdditionalRxCredits - status: %s, "
        "additional_rx_credits: %u, rx_total_credits_: %u, "
        "rx_remaining_credits_: %u",
        status.str(),
        additional_rx_credits,
        rx_total_credits_,
        rx_remaining_credits_);
  }
  return status;
}

void L2capCoc::ProcessPduFromControllerMultibuf(span<uint8_t> kframe) {
  std::lock_guard lock(rx_mutex_);
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
          "(CID %#x) Buffer is too small for first K-frame. So stopping "
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
          "(CID %#x) Rx MultiBuf allocator out of memory. So stopping channel "
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
        "(CID %#x) Sum of K-frame payload sizes exceeds the specified SDU "
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

bool L2capCoc::DoHandlePduFromController(pw::span<uint8_t> kframe) {
  if (state() != State::kRunning) {
    PW_LOG_ERROR(
        "btproxy: L2capCoc::HandlePduFromController on non-running "
        "channel. local_cid: %u, remote_cid: %u, state: %u",
        local_cid(),
        remote_cid(),
        cpp23::to_underlying(state()));
    StopAndSendEvent(L2capChannelEvent::kRxWhileStopped);
    return true;
  }

  {
    std::lock_guard lock(rx_mutex_);
    rx_remaining_credits_--;

    uint16_t rx_credits_used = rx_total_credits_ - rx_remaining_credits_;
    if (rx_credits_used >=
        std::ceil(rx_total_credits_ * kRxCreditReplenishThreshold)) {
      Status status = ReplenishRxCredits(rx_credits_used);
      if (status.IsUnavailable()) {
        PW_LOG_INFO(
            "Unable to send %hu rx credits to remote (it has %hu credits "
            "remaining). Will try on next PDU receive.",
            rx_credits_used,
            rx_total_credits_);
      } else if (status.IsFailedPrecondition()) {
        PW_LOG_WARN(
            "Unable to send rx credits to remote, perhaps the connection has "
            "been closed?");
      } else {
        PW_CHECK(status.ok());
        rx_remaining_credits_ += rx_credits_used;
      }
    }
  }

  // TODO: https://pwbug.dev/369849508 - Make this the only path once clients
  // move to MultiBuf Write() API.
  if (receive_fn_multibuf_) {
    ProcessPduFromControllerMultibuf(kframe);
    return true;
  }

  std::lock_guard lock(rx_mutex_);
  // If `remaining_sdu_bytes_to_ignore_` is nonzero, we are in state where we
  // are dropping continuing PDUs in a segmented SDU.
  if (remaining_sdu_bytes_to_ignore_ > 0) {
    Result<emboss::SubsequentKFrameView> subsequent_kframe_view =
        MakeEmbossView<emboss::SubsequentKFrameView>(kframe);
    if (!subsequent_kframe_view.ok()) {
      PW_LOG_ERROR(
          "(CID %#x) Buffer is too small for subsequent L2CAP K-frame. So "
          "will drop.",
          local_cid());
      return true;
    }
    PW_LOG_INFO("(CID %#x) Dropping PDU that is part of current segmented SDU.",
                local_cid());
    if (subsequent_kframe_view->payload_size().Read() >
        remaining_sdu_bytes_to_ignore_) {
      // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the sum of the payload sizes
      // for the K-frames exceeds the specified SDU length, the receiver shall
      // disconnect the channel."
      PW_LOG_ERROR(
          "(CID %#x) Sum of K-frame payload sizes exceeds the specified SDU "
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
        "(CID %#x) Buffer is too small for L2CAP K-frame. So stopping channel "
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
        "(CID %#x) Rx K-frame SDU exceeds MTU. So stopping channel & "
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
        "(CID %#x) Encountered segmented L2CAP SDU (which is not yet "
        "supported). So will drop all PDUs in SDU.",
        local_cid());
    remaining_sdu_bytes_to_ignore_ = sdu_length - payload_size;
    return true;
  }

  // Core Spec v6.0 Vol 3, Part A, 3.4.3: "If the payload size of any K-frame
  // exceeds the receiver's MPS, the receiver shall disconnect the channel."
  if (payload_size > rx_mps_) {
    PW_LOG_ERROR(
        "(CID %#x) Rx K-frame payload exceeds MPU. So stopping channel & "
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

bool L2capCoc::HandlePduFromHost(pw::span<uint8_t>) {
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
          /*payload_from_host_fn=*/nullptr,
          /*event_fn=*/std::move(event_fn)),
      rx_multibuf_allocator_(rx_multibuf_allocator),
      signaling_channel_(signaling_channel),
      rx_mtu_(rx_config.mtu),
      rx_mps_(rx_config.mps),
      tx_mtu_(tx_config.mtu),
      tx_mps_(tx_config.mps),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)),
      receive_fn_multibuf_(std::move(receive_fn_multibuf)),
      rx_remaining_credits_(rx_config.credits),
      rx_total_credits_(rx_config.credits),
      tx_credits_(tx_config.credits) {
  PW_LOG_INFO(
      "btproxy: L2capCoc ctor - rx_remaining_credits_: %u, "
      "rx_total_credits_: %u, tx_credits_: %u",
      rx_remaining_credits_,
      rx_total_credits_,
      tx_credits_);
}

L2capCoc::~L2capCoc() {
  // Don't log dtor of moved-from channels.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: L2capCoc dtor");
  }
}

std::optional<uint16_t> L2capCoc::MaxL2capPayloadSize() const {
  std::optional<uint16_t> max_l2cap_payload_size =
      L2capChannel::MaxL2capPayloadSize();
  if (!max_l2cap_payload_size) {
    return std::nullopt;
  }
  return std::min(*max_l2cap_payload_size, tx_mps_);
}

std::optional<H4PacketWithH4> L2capCoc::GenerateNextTxPacket() {
  std::lock_guard lock(tx_mutex_);
  constexpr uint8_t kSduLengthFieldSize = 2;
  std::optional<uint16_t> max_l2cap_payload_size = MaxL2capPayloadSize();
  if (state() != State::kRunning || PayloadQueueEmpty() || tx_credits_ == 0 ||
      !max_l2cap_payload_size ||
      *max_l2cap_payload_size <= kSduLengthFieldSize) {
    return std::nullopt;
  }

  ConstByteSpan sdu_span = GetFrontPayloadSpan();
  // Number of client SDU bytes to be encoded in this segment.
  uint16_t sdu_bytes_in_segment;
  // Size of PDU payload for this L2CAP frame.
  uint16_t pdu_data_size;
  if (!is_continuing_segment_) {
    // Generating the first (or only) PDU of an SDU.
    size_t sdu_bytes_max_allowable =
        *max_l2cap_payload_size - kSduLengthFieldSize;
    sdu_bytes_in_segment = std::min(sdu_span.size(), sdu_bytes_max_allowable);
    pdu_data_size = sdu_bytes_in_segment + kSduLengthFieldSize;
  } else {
    // Generating a continuing PDU in an SDU.
    size_t sdu_bytes_max_allowable = *max_l2cap_payload_size;
    sdu_bytes_in_segment =
        std::min(sdu_span.size() - tx_sdu_offset_, sdu_bytes_max_allowable);
    pdu_data_size = sdu_bytes_in_segment;
  }

  pw::Result<H4PacketWithH4> h4_result = PopulateTxL2capPacket(pdu_data_size);
  if (!h4_result.ok()) {
    // This can fail if all H4 buffers are occupied.
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK(acl.ok());

  if (!is_continuing_segment_) {
    Result<emboss::FirstKFrameWriter> first_kframe_writer =
        MakeEmbossWriter<emboss::FirstKFrameWriter>(
            acl->payload().BackingStorage().data(),
            acl->payload().SizeInBytes());
    PW_CHECK(first_kframe_writer.ok());
    first_kframe_writer->sdu_length().Write(sdu_span.size());
    PW_CHECK(first_kframe_writer->Ok());
    PW_CHECK(TryToCopyToEmbossStruct(
        /*emboss_dest=*/first_kframe_writer->payload(),
        /*src=*/sdu_span.subspan(tx_sdu_offset_, sdu_bytes_in_segment)));
  } else {
    Result<emboss::SubsequentKFrameWriter> subsequent_kframe_writer =
        MakeEmbossWriter<emboss::SubsequentKFrameWriter>(
            acl->payload().BackingStorage().data(),
            acl->payload().SizeInBytes());
    PW_CHECK(subsequent_kframe_writer.ok());
    PW_CHECK(TryToCopyToEmbossStruct(
        /*emboss_dest=*/subsequent_kframe_writer->payload(),
        /*src=*/sdu_span.subspan(tx_sdu_offset_, sdu_bytes_in_segment)));
  }

  tx_sdu_offset_ += sdu_bytes_in_segment;

  if (tx_sdu_offset_ == sdu_span.size()) {
    // This segment was the final (or only) PDU of the SDU.
    PopFrontPayload();
    tx_sdu_offset_ = 0;
    is_continuing_segment_ = false;
  } else {
    is_continuing_segment_ = true;
  }

  --tx_credits_;
  return h4_packet;
}

void L2capCoc::AddTxCredits(uint16_t credits) {
  if (state() != State::kRunning) {
    PW_LOG_ERROR(
        "(CID %#x) Received credits on stopped CoC. So will ignore signal.",
        local_cid());
    return;
  }

  bool credits_previously_zero;
  {
    std::lock_guard lock(tx_mutex_);

    // Core Spec v6.0 Vol 3, Part A, 10.1: "The device receiving the credit
    // packet shall disconnect the L2CAP channel if the credit count exceeds
    // 65535."
    if (credits > emboss::L2capLeCreditBasedConnectionReq::max_credit_value() -
                      tx_credits_) {
      PW_LOG_ERROR(
          "btproxy: Received additional tx credits %u which put tx_credits_ %u "
          "beyond max credit value of %ld. So stopping channel and reporting "
          "it needs to be closed. local_cid: %u, remote_cid: %u",
          credits,
          tx_credits_,
          long{emboss::L2capLeCreditBasedConnectionReq::max_credit_value()},
          local_cid(),
          remote_cid());
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
