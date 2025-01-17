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

#include "pw_bluetooth_sapphire/internal/host/l2cap/credit_based_flow_control_rx_engine.h"

#include <pw_bluetooth/l2cap_frames.emb.h>

namespace bt::l2cap::internal {

namespace emboss = pw::bluetooth::emboss;
constexpr auto kSduHeaderSize =
    pw::bluetooth::emboss::KFrameSduHeader::IntrinsicSizeInBytes();

CreditBasedFlowControlRxEngine::CreditBasedFlowControlRxEngine(
    FailureCallback failure_callback)
    : failure_callback_(std::move(failure_callback)) {}

ByteBufferPtr CreditBasedFlowControlRxEngine::ProcessPdu(PDU pdu) {
  if (!pdu.is_valid()) {
    OnFailure();
    return nullptr;
  }

  size_t sdu_offset = 0;
  if (!next_sdu_) {
    if (pdu.length() < kSduHeaderSize) {
      // This is a PDU containing the start of a new K-Frame SDU, but the
      // payload isn't large enough to contain the SDU size field as required
      // by the spec.
      OnFailure();
      return nullptr;
    }

    StaticByteBuffer<kSduHeaderSize> sdu_size_buffer;
    pdu.Copy(&sdu_size_buffer, 0, kSduHeaderSize);
    auto sdu_size =
        emboss::MakeKFrameSduHeaderView(&sdu_size_buffer).sdu_length().Read();

    next_sdu_ = std::make_unique<DynamicByteBuffer>(sdu_size);

    // Skip the SDU header when copying the payload.
    sdu_offset = kSduHeaderSize;
  }

  if (valid_bytes_ + pdu.length() - sdu_offset > next_sdu_->size()) {
    // Invalid PDU is larger than the number of remaining bytes in the SDU.
    OnFailure();
    return nullptr;
  }
  auto view = next_sdu_->mutable_view(valid_bytes_);
  valid_bytes_ += pdu.Copy(&view, sdu_offset);

  if (valid_bytes_ < next_sdu_->size()) {
    // Segmented SDU, wait for additional PDU(s) to complete.
    return nullptr;
  }

  valid_bytes_ = 0;
  return std::move(next_sdu_);
}

void CreditBasedFlowControlRxEngine::OnFailure() {
  failure_callback_();
  valid_bytes_ = 0;
  next_sdu_ = nullptr;
}

}  // namespace bt::l2cap::internal
