// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/basic_mode_rx_engine.h"

#include <zircon/assert.h>

namespace bt {
namespace l2cap {
namespace internal {

common::ByteBufferPtr BasicModeRxEngine::ProcessPdu(PDU pdu) {
  ZX_ASSERT(pdu.is_valid());
  auto sdu = std::make_unique<common::DynamicByteBuffer>(pdu.length());
  pdu.Copy(sdu.get());
  return sdu;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
