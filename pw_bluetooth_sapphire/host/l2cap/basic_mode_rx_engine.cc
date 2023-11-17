// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/l2cap/basic_mode_rx_engine.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"

namespace bt::l2cap::internal {

ByteBufferPtr BasicModeRxEngine::ProcessPdu(PDU pdu) {
  BT_ASSERT(pdu.is_valid());
  auto sdu = std::make_unique<DynamicByteBuffer>(pdu.length());
  pdu.Copy(sdu.get());
  return sdu;
}

}  // namespace bt::l2cap::internal
