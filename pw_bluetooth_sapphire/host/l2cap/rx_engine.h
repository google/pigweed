// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/pdu.h"

namespace bt::l2cap::internal {

// The interface between a Channel, and the module implementing the
// mode-specific receive logic. The primary purpose of an RxEngine is to
// transform PDUs into SDUs. See Bluetooth Core Spec v5.0, Volume 3, Part A,
// Sec 2.4, "Modes of Operation" for more information about the possible modes.
class RxEngine {
 public:
  RxEngine() = default;
  virtual ~RxEngine() = default;

  // Consumes a PDU and returns a buffer containing the resulting SDU. Returns
  // nullptr if no SDU was produced.
  //
  // Notes:
  // * Callers should not interpret a nullptr as an error, as there are many
  //   valid conditions under which a PDU does not yield an SDU.
  // * The caller must ensure that |pdu.is_valid() == true|.
  virtual ByteBufferPtr ProcessPdu(PDU pdu) = 0;

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(RxEngine);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_RX_ENGINE_H_
