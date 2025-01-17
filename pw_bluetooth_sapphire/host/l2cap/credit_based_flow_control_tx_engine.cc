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

#include "pw_bluetooth_sapphire/internal/host/l2cap/credit_based_flow_control_tx_engine.h"

#include <pw_assert/check.h>
#include <pw_bluetooth/l2cap_frames.emb.h>

#include <algorithm>

#include "pw_bluetooth_sapphire/internal/host/common/log.h"

namespace bt::l2cap::internal {
namespace {

namespace emboss = pw::bluetooth::emboss;

constexpr auto kLeCreditBasedFlowControlMode =
    CreditBasedFlowControlMode::kLeCreditBasedFlowControl;

constexpr size_t kMinimumLeMtu = 23;
constexpr size_t kMinimumLeMps = 23;
constexpr auto kPduHeaderSize = emboss::KFramePduHeader::IntrinsicSizeInBytes();
constexpr auto kSduHeaderSize = emboss::KFrameSduHeader::IntrinsicSizeInBytes();

constexpr uint32_t kMaxCredits = 65535;

template <typename Enum>
constexpr typename std::underlying_type<Enum>::type EnumValue(const Enum& e) {
  return static_cast<typename std::underlying_type<Enum>::type>(e);
}

// Returns the payload size of the next PDU needed to transmit the maximal
// amount of the remaining |total_payload_size| bytes.
uint16_t NextPduPayloadSize(size_t total_payload_size, uint16_t max_pdu_size) {
  PW_DCHECK(max_pdu_size > kPduHeaderSize);
  // Factor in the header size.
  uint16_t max_payload_size = max_pdu_size - kPduHeaderSize;
  // There is no risk of overflow in this static cast as any value of
  // `total_payload_size` larger than uint16_t max will be bounded by the
  // smaller value in `max_payload_size`.
  return static_cast<uint16_t>(
      std::min<size_t>(total_payload_size, max_payload_size));
}

// Create and initialize a K-Frame with appropriate PDU header.
// Returns a tuple of the frame itself and a view of the payload.
std::tuple<DynamicByteBuffer, MutableBufferView> CreateFrame(
    uint16_t payload_size, uint16_t channel_id) {
  uint16_t pdu_size = kPduHeaderSize + payload_size;

  DynamicByteBuffer buf(pdu_size);
  auto header =
      emboss::KFramePduHeaderWriter(buf.mutable_data(), kPduHeaderSize);
  header.pdu_length().Write(payload_size);
  header.channel_id().Write(channel_id);

  MutableBufferView payload = buf.mutable_view(kPduHeaderSize);
  return std::make_tuple(std::move(buf), payload);
}
}  // namespace

CreditBasedFlowControlTxEngine::CreditBasedFlowControlTxEngine(
    ChannelId channel_id,
    uint16_t max_tx_sdu_size,
    TxChannel& channel,
    CreditBasedFlowControlMode mode,
    uint16_t max_tx_pdu_size,
    uint16_t initial_credits)
    : TxEngine(channel_id, max_tx_sdu_size, channel),
      mode_(mode),
      max_tx_pdu_size_(max_tx_pdu_size),
      credits_(initial_credits) {
  // The enhanced flow control mode is not yet supported.
  PW_CHECK(mode_ == kLeCreditBasedFlowControlMode,
           "Credit based flow control mode unsupported: 0x%.2ux",
           EnumValue(mode));

  PW_DCHECK(
      mode != kLeCreditBasedFlowControlMode || max_tx_sdu_size > kMinimumLeMtu,
      "Invalid MTU for LE mode: %d",
      max_tx_sdu_size);
  PW_DCHECK(
      mode != kLeCreditBasedFlowControlMode || max_tx_pdu_size > kMinimumLeMps,
      "Invalid MPS for LE mode: %d",
      max_tx_pdu_size);
}

CreditBasedFlowControlTxEngine::~CreditBasedFlowControlTxEngine() = default;

void CreditBasedFlowControlTxEngine::NotifySduQueued() { ProcessSdus(); }

bool CreditBasedFlowControlTxEngine::AddCredits(uint16_t credits) {
  if (static_cast<uint32_t>(credits_) + credits > kMaxCredits)
    return false;

  credits_ += credits;

  // If there are queued SDUs, use the newly added credits to send them.
  ProcessSdus();
  return true;
}

uint16_t CreditBasedFlowControlTxEngine::credits() const { return credits_; }

size_t CreditBasedFlowControlTxEngine::segments_count() const {
  return segments_.size();
}

void CreditBasedFlowControlTxEngine::SegmentSdu(ByteBufferPtr sdu) {
  size_t payload_remaining = sdu->size() + kSduHeaderSize;

  do {
    // A credit based flow control packet has a fairly simple segmentation
    // scheme where each PDU contains (at most) |max_tx_pdu_size_| - header_size
    // bytes. The only bit of (relatively minor) complexity is the SDU size
    // field, which is only included in the first K-Frame/PDU of the SDU. This
    // loop iterates over each potential packet (and will only execute once if
    // the entire SDU fits in a single PDU).

    DynamicByteBuffer frame;
    MutableBufferView payload;
    std::tie(frame, payload) = CreateFrame(
        NextPduPayloadSize(payload_remaining, max_tx_pdu_size_), channel_id());
    PW_DCHECK(payload.size() <= payload_remaining);

    if (payload_remaining > sdu->size()) {
      // First frame of the SDU, write the SDU header.
      emboss::KFrameSduHeaderWriter header(payload.mutable_data(),
                                           kSduHeaderSize);
      header.sdu_length().Write(sdu->size());

      payload = payload.mutable_view(kSduHeaderSize);
      payload_remaining -= kSduHeaderSize;
      PW_DCHECK(payload_remaining == sdu->size());
    }

    sdu->Copy(&payload, sdu->size() - payload_remaining, payload.size());
    payload_remaining -= payload.size();

    segments_.push_back(std::make_unique<DynamicByteBuffer>(std::move(frame)));
  } while (payload_remaining > 0);
}

void CreditBasedFlowControlTxEngine::TrySendSegments() {
  while (credits_ > 0 && !segments_.empty()) {
    channel().SendFrame(std::move(segments_.front()));
    segments_.pop_front();
    --credits_;
  }
}

void CreditBasedFlowControlTxEngine::ProcessSdus() {
  TrySendSegments();
  while (credits_ > 0) {
    std::optional<ByteBufferPtr> sdu = channel().GetNextQueuedSdu();

    if (!sdu)
      break;
    PW_CHECK(*sdu);

    if ((*sdu)->size() > max_tx_sdu_size()) {
      bt_log(INFO,
             "l2cap",
             "SDU size exceeds channel TxMTU (channel-id: 0x%.4x)",
             channel_id());
      return;
    }

    SegmentSdu(std::move(*sdu));
    TrySendSegments();
  }
}

}  // namespace bt::l2cap::internal
