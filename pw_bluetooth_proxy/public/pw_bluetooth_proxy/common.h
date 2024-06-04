// Copyright 2024 The Pigweed Authors
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

#pragma once

#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

// An H4 HCI packet. `h4_type` should be the HCI packet type indicator as
// defined in BT Core Spec Version 5.4 | Vol 4, Part A, Section 2. The
// `hci_span` should be an HCI packet as defined in BT Core Spec Version 5.4 |
// Vol 4, Part E, Section 5.4.
struct H4HciPacket {
  emboss::H4PacketType h4_type;
  pw::span<uint8_t> hci_span;
};

using H4HciPacketSendFn = pw::Function<void(H4HciPacket packet)>;

}  // namespace pw::bluetooth::proxy
