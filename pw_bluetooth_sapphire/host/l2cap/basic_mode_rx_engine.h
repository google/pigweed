// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BASIC_MODE_RX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BASIC_MODE_RX_ENGINE_H_

#include "src/connectivity/bluetooth/core/bt-host/l2cap/rx_engine.h"

namespace bt::l2cap::internal {

// Implements the receiver-side functionality of L2CAP Basic Mode. See Bluetooth
// Core Spec v5.0, Volume 3, Part A, Sec 2.4, "Modes of Operation".
class BasicModeRxEngine final : public RxEngine {
 public:
  BasicModeRxEngine() = default;
  virtual ~BasicModeRxEngine() = default;

  ByteBufferPtr ProcessPdu(PDU) override;

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BasicModeRxEngine);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BASIC_MODE_RX_ENGINE_H_
