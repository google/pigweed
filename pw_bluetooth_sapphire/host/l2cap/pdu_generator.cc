// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdu_generator.h"

#include <zircon/assert.h>

#include <algorithm>

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt {
namespace l2cap {
namespace internal {

namespace {

constexpr static hci::ConnectionHandle kTestHandle = 0x0001;
constexpr static ChannelId kTestChannelId = 0x0001;

}  // namespace

PduGenerator::PduGenerator(const uint8_t* input_buf, size_t buflen,
                           size_t num_pdus)
    : input_buf_(input_buf),
      buf_len_bytes_(buflen),
      pdu_len_bytes_(std::min<size_t>(num_pdus ? buflen / num_pdus : buflen + 1,
                                      kMaxBasicFramePayloadSize)),
      fragmenter_(kTestHandle),
      input_pos_(0) {
  ZX_ASSERT(input_buf_);
}

std::optional<PDU> PduGenerator::GetNextPdu() {
  if (pdu_len_bytes_ == 0) {
    // Avoid infinite-loop, if the provided buffer is too small to provide at
    // least 1 byte for each PDU.
    return std::nullopt;
  }

  ZX_ASSERT(input_pos_ <= buf_len_bytes_);  // Ensure no underflow below.
  const size_t avail = buf_len_bytes_ - input_pos_;
  if (pdu_len_bytes_ > avail) {
    return std::nullopt;
  }

  // For the rationale behind using a Fragmenter to build the PDU, see the
  // comment in basic_mode_rx_engine_fuzztest.cc.
  auto pdu = fragmenter_.BuildBasicFrame(
      kTestChannelId, BufferView(input_buf_ + input_pos_, pdu_len_bytes_));
  input_pos_ += pdu_len_bytes_;
  return std::move(pdu);
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
