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

#include "pw_bluetooth_sapphire/internal/host/l2cap/basic_mode_tx_engine.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

namespace bt::l2cap::internal {

bool BasicModeTxEngine::QueueSdu(ByteBufferPtr sdu) {
  BT_ASSERT(sdu);
  if (sdu->size() > max_tx_sdu_size_) {
    bt_log(INFO,
           "l2cap",
           "SDU size exceeds channel TxMTU (channel-id: %#.4x)",
           channel_id_);
    return false;
  }
  send_frame_callback_(std::move(sdu));
  return true;
}

}  // namespace bt::l2cap::internal
