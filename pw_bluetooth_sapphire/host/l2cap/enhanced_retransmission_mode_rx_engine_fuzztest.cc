// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enhanced_retransmission_mode_rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu_generator.h"

BT_DECLARE_FAKE_DRIVER();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    // We need at least one byte, to know how many PDUs to generate.
    return 0;
  }

  // We use the first byte of |data| to determine the number of PDUs. The
  // upper-bound of 256 PDUs is probably large enough to be interesting (e.g.,
  // forces |next_seqnum_| to roll-over), but without requiring too much time
  // per call from the fuzzer.
  const uint8_t num_pdus = data[0];
  bt::l2cap::internal::PduGenerator pdu_generator(data + 1, size - 1, num_pdus);
  bt::l2cap::internal::EnhancedRetransmissionModeRxEngine rx_engine(
      [](auto) {});
  while (auto pdu = pdu_generator.GetNextPdu()) {
    rx_engine.ProcessPdu(std::move(pdu.value()));
  }
  return 0;
}
