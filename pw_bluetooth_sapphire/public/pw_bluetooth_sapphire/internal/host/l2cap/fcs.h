// Copyright 2023 The Pigweed Authors
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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_

#include <cstddef>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap {

// Computes the Frame Check Sequence (FCS) over the bytes in |view| per Core
// Spec v5.0, Vol 3, Part A, Section 3.3.5. |initial_value| may be
// kInitialFcsValue to begin a new FCS computation or the result of FCS
// computation over a contiguous sequence immediately preceding |view|. If
// |view| has length zero, returns |initial_value|.
[[nodiscard]] FrameCheckSequence ComputeFcs(
    BufferView view, FrameCheckSequence initial_value = kInitialFcsValue);

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FCS_H_
