// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "basic_mode_rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"

BT_DECLARE_FAKE_DRIVER();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr bt::hci::ConnectionHandle kTestHandle = 0x0001;
  constexpr bt::l2cap::ChannelId kTestChannelId = 0x0001;
  bt::l2cap::Fragmenter fragmenter(kTestHandle);
  bt::l2cap::internal::BasicModeRxEngine rx_engine;

  // The use of a fragmenter, to build a PDU for the receive engine, is
  // admittedly counterintuitive. (In actual operation, we use a Fragmenter on
  // the transmit path, and a Recombiner on the receive path.) Pragmatically,
  // however, the Fragmenter is the easiest way to build a PDU.
  //
  // Note that using a Fragmenter to build the PDU doesn't decrease the efficacy
  // of fuzzing, because the only guarantees provided by the Fragmenter are
  // those that are preconditions for RxEngine::ProcessPdu().
  auto pdu = fragmenter.BuildFrame(kTestChannelId, bt::BufferView(data, size),
                                   bt::l2cap::FrameCheckSequenceOption::kNoFcs);
  rx_engine.ProcessPdu(std::move(pdu));
  return 0;
}
