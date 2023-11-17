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
