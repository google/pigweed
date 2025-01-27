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

#include "pw_bluetooth_proxy/rfcomm_channel.h"

#include <mutex>

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth/rfcomm_frames.emb.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/internal/rfcomm_fcs.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

RfcommChannel::RfcommChannel(RfcommChannel&& other)
    : L2capChannel(static_cast<RfcommChannel&&>(other)),
      rx_config_(other.rx_config_),
      tx_config_(other.tx_config_),
      channel_number_(other.channel_number_),
      payload_from_controller_multibuf_fn_(
          std::move(other.payload_from_controller_multibuf_fn_)),
      payload_from_controller_fn_(
          std::move(other.payload_from_controller_fn_)) {
  {
    std::lock_guard lock(rx_mutex_);
    std::lock_guard other_lock(other.rx_mutex_);
    rx_credits_ = other.rx_credits_;
  }
  {
    std::lock_guard lock(tx_mutex_);
    std::lock_guard other_lock(other.tx_mutex_);
    tx_credits_ = other.tx_credits_;
  }
}

namespace {

// We always encode credits.
constexpr size_t kCreditsFieldSize = 1;

}  // namespace

StatusWithMultiBuf RfcommChannel::Write(multibuf::MultiBuf&& payload) {
  if (payload.size() > tx_config_.max_information_length - kCreditsFieldSize) {
    PW_LOG_WARN("Payload (%zu bytes) is too large. So will not process.",
                payload.size());
    return {Status::InvalidArgument(), std::move(payload)};
  }

  return L2capChannel::Write(std::move(payload));
}

std::optional<H4PacketWithH4> RfcommChannel::GenerateNextTxPacket() {
  if (state() != State::kRunning || PayloadQueueEmpty()) {
    return std::nullopt;
  }

  ConstByteSpan payload = GetFrontPayloadSpan();

  constexpr size_t kMaxShortLength = 0x7f;

  const bool uses_extended_length = payload.size() > kMaxShortLength;
  const size_t length_extended_size = uses_extended_length ? 1 : 0;
  const size_t frame_size = emboss::RfcommFrame::MinSizeInBytes() +
                            length_extended_size + kCreditsFieldSize +
                            payload.size();

  // TODO: https://pwbug.dev/379337260 - Support fragmentation.
  pw::Result<H4PacketWithH4> h4_result = PopulateTxL2capPacket(frame_size);
  if (!h4_result.ok()) {
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(*h4_result);

  Result<emboss::AclDataFrameWriter> result2 =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK(result2.ok());
  emboss::AclDataFrameWriter acl = result2.value();

  // At this point we assume we can return a PDU with the payload.
  PopFrontPayload();

  emboss::BFrameWriter bframe = emboss::MakeBFrameView(
      acl.payload().BackingStorage().data(), acl.payload().SizeInBytes());
  PW_CHECK(bframe.IsComplete());

  emboss::RfcommFrameWriter rfcomm = emboss::MakeRfcommFrameView(
      bframe.payload().BackingStorage().data(), bframe.payload().SizeInBytes());
  PW_CHECK(bframe.payload().SizeInBytes() >= frame_size);

  rfcomm.extended_address().Write(true);
  // TODO: https://pwbug.dev/378691959 - Sniff correct C/R/D from Multiplexer
  // control commands on RFCOMM channel 0
  rfcomm.command_response_direction().Write(
      emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_RESPONDER);
  rfcomm.channel().Write(channel_number_);

  // Poll/Final = 1 indicates Credits present.
  rfcomm.control().Write(
      emboss::RfcommFrameType::
          UNNUMBERED_INFORMATION_WITH_HEADER_CHECK_AND_POLL_FINAL);
  PW_CHECK(rfcomm.has_credits().ValueOrDefault());

  if (!uses_extended_length) {
    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
    rfcomm.length().Write(payload.size());
  } else {
    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::EXTENDED);
    rfcomm.length_extended().Write(payload.size());
  }

  {
    std::lock_guard lock(rx_mutex_);
    // TODO: https://pwbug.dev/379184978 - Refill remote side with credits they
    // have sent. We assume our receiver can handle data without need for
    // blocking. Revisit when adding downstream flow control to this API.
    const uint8_t to_refill = rx_config_.credits - rx_credits_;
    rfcomm.credits().Write(to_refill);
    rx_credits_ = rx_config_.credits;
  }

  if (rfcomm.information().SizeInBytes() < payload.size()) {
    return std::nullopt;
  }
  PW_CHECK(rfcomm.information().SizeInBytes() == payload.size());
  PW_CHECK(TryToCopyToEmbossStruct(
      /*emboss_dest=*/rfcomm.information(),
      /*src=*/payload));

  // UIH frame type:
  //   FCS should be calculated over address and control fields.
  rfcomm.fcs().Write(RfcommFcs(rfcomm));

  PW_CHECK(acl.Ok());
  PW_CHECK(bframe.Ok());
  PW_CHECK(rfcomm.Ok());

  return h4_packet;
}

std::optional<H4PacketWithH4> RfcommChannel::DequeuePacket() {
  std::lock_guard lock(tx_mutex_);
  if (tx_credits_ == 0) {
    return std::nullopt;
  }

  std::optional<H4PacketWithH4> maybe_packet = L2capChannel::DequeuePacket();
  if (maybe_packet.has_value()) {
    --tx_credits_;
  }
  return maybe_packet;
}

Result<RfcommChannel> RfcommChannel::Create(
    L2capChannelManager& l2cap_channel_manager,
    multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    Config rx_config,
    Config tx_config,
    uint8_t channel_number,
    Function<void(multibuf::MultiBuf&& payload)>&&
        payload_from_controller_multibuf_fn,
    Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
  if (!AreValidParameters(/*connection_handle=*/connection_handle,
                          /*local_cid=*/rx_config.cid,
                          /*remote_cid=*/tx_config.cid)) {
    return Status::InvalidArgument();
  }

  return RfcommChannel(l2cap_channel_manager,
                       rx_multibuf_allocator,
                       connection_handle,
                       rx_config,
                       tx_config,
                       channel_number,
                       std::move(payload_from_controller_multibuf_fn),
                       std::move(payload_from_controller_fn),
                       std::move(event_fn));
}

bool RfcommChannel::DoHandlePduFromController(pw::span<uint8_t> l2cap_pdu) {
  if (state() != State::kRunning) {
    PW_LOG_WARN("Received data on stopped channel, passing on to host.");
    return false;
  }

  Result<emboss::BFrameView> bframe_view =
      MakeEmbossView<emboss::BFrameView>(l2cap_pdu);
  if (!bframe_view.ok()) {
    PW_LOG_ERROR(
        "(CID %#x) Buffer is too small for L2CAP B-frame, passing on to host.",
        local_cid());
    return false;
  }

  Result<emboss::RfcommFrameView> rfcomm_view =
      MakeEmbossView<emboss::RfcommFrameView>(
          bframe_view->payload().BackingStorage().data(),
          bframe_view->payload().SizeInBytes());
  if (!rfcomm_view.ok()) {
    PW_LOG_ERROR("Unable to parse RFCOMM frame, passing on to host.");
    return false;
  }

  if (rfcomm_view->channel().Read() == 0 || !rfcomm_view->uih().Read()) {
    // Ignore control frames.
    return false;
  }

  const uint8_t fcs = RfcommFcs(*rfcomm_view);
  if (rfcomm_view->fcs().Read() != fcs) {
    PW_LOG_ERROR("Bad checksum %02X (exp %02X), passing on to host.",
                 rfcomm_view->fcs().Read(),
                 fcs);
    return false;
  }

  // TODO: https://pwbug.dev/378691959 - Validate channel, control, C/R,
  // direction is what is expected.

  if (rfcomm_view->channel().Read() != channel_number_) {
    PW_LOG_WARN("RFCOMM data not for our channel %d (%d)",
                rfcomm_view->channel().Read(),
                channel_number_);
  }

  bool credits_previously_zero = false;
  {
    std::lock_guard lock(tx_mutex_);
    credits_previously_zero = tx_credits_ == 0;
    if (rfcomm_view->has_credits().ValueOrDefault()) {
      tx_credits_ += rfcomm_view->credits().Read();
    }
  }

  pw::span<uint8_t> information = pw::span(
      const_cast<uint8_t*>(rfcomm_view->information().BackingStorage().data()),
      rfcomm_view->information().SizeInBytes());

  SendPayloadFromControllerToClient(information);

  bool rx_needs_refill = false;
  {
    std::lock_guard lock(rx_mutex_);
    if (rx_credits_ == 0) {
      PW_LOG_ERROR("Received frame with no rx credits available.");
      // TODO: https://pwbug.dev/379184978 - Consider dropping channel since
      // this is invalid state.
    } else {
      --rx_credits_;
    }
    rx_needs_refill = rx_credits_ < kMinRxCredits;
  }

  if (rx_needs_refill) {
    // Send credit update with empty payload to refresh remote credit count.
    if (const auto status = Write(pw::multibuf::MultiBuf{}).status;
        !status.ok()) {
      PW_LOG_ERROR("Failed to send RFCOMM credits");
    }
  }

  if (credits_previously_zero) {
    ReportPacketsMayBeReadyToSend();
  }

  return true;
}

bool RfcommChannel::SendPayloadFromControllerToClient(
    pw::span<uint8_t> payload) {
  if (payload_from_controller_fn_) {
    payload_from_controller_fn_(payload);
  } else if (payload_from_controller_multibuf_fn_) {
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

    payload_from_controller_multibuf_fn_(std::move(*buffer));
  }
  return true;
}

bool RfcommChannel::HandlePduFromHost(pw::span<uint8_t>) { return false; }

RfcommChannel::RfcommChannel(
    L2capChannelManager& l2cap_channel_manager,
    multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    Config rx_config,
    Config tx_config,
    uint8_t channel_number,
    Function<void(multibuf::MultiBuf&& payload)>&&
        payload_from_controller_multibuf_fn,
    Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn)
    : L2capChannel(l2cap_channel_manager,
                   &rx_multibuf_allocator,
                   /*connection_handle=*/connection_handle,
                   /*transport=*/AclTransportType::kBrEdr,
                   /*local_cid=*/rx_config.cid,
                   /*remote_cid=*/tx_config.cid,
                   /*payload_from_controller_multibuf_fn=*/nullptr,
                   /*payload_from_host_multibuf_fn=*/nullptr,
                   /*payload_from_controller_fn=*/nullptr,
                   /*payload_from_host_fn=*/nullptr,
                   /*event_fn=*/std::move(event_fn)),
      rx_config_(rx_config),
      tx_config_(tx_config),
      channel_number_(channel_number),
      rx_credits_(rx_config.credits),
      tx_credits_(tx_config.credits),
      payload_from_controller_multibuf_fn_(
          std::move(payload_from_controller_multibuf_fn)),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)) {
  PW_LOG_INFO(
      "btproxy: RfcommChannel ctor - channel_number_: %u, rx_credits_: %u, "
      "tx_credits_: %u",
      channel_number_,
      rx_credits_,
      tx_credits_);
}

RfcommChannel::~RfcommChannel() {
  // Don't log dtor of moved-from channels.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: RfcommChannel dtor - channel_number_: %u",
                channel_number_);
  }
}

}  // namespace pw::bluetooth::proxy
