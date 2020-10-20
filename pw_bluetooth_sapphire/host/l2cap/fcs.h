// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_

#include <cstddef>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt {
namespace l2cap {

// Computes the Frame Check Sequence (FCS) over the bytes in |view| per Core Spec v5.0, Vol 3, Part
// A, Section 3.3.5. |initial_value| may be kInitialFcsValue to begin a new FCS computation or the
// result of FCS computation over a contiguous sequence immediately preceding |view|. If |view| has
// length zero, returns |initial_value|.
[[nodiscard]] FrameCheckSequence ComputeFcs(BufferView view,
                                            FrameCheckSequence initial_value = kInitialFcsValue);

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_
