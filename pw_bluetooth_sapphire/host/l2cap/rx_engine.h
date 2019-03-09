// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu.h"

namespace btlib {
namespace l2cap {
namespace internal {

// The interface between a Channel, and the module implementing the
// mode-specific receive logic. The primary purpose of an RxEngine is to
// transform PDUs into SDUs. See Bluetooth Core Spec v5.0, Volume 3, Part A,
// Sec 2.4, "Modes of Operation" for more information about the possible modes.
class RxEngine {
 public:
  RxEngine() = default;
  virtual ~RxEngine() = default;

  // Consumes a PDU and returns a buffer containing the resulting SDU. Returns
  // nullptr if no SDU was produced. Callers should not interpret a nullptr as
  // an error, as there are many valid conditions under which a PDU does not
  // yield an SDU.
  virtual common::ByteBufferPtr ProcessPdu(PDU) = 0;

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(RxEngine);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_
