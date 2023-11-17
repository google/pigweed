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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/l2cap/tx_engine.h"

namespace bt::l2cap::internal {

// Implements the sender-side functionality of L2CAP Basic Mode. See Bluetooth
// Core Spec v5.0, Volume 3, Part A, Sec 2.4, "Modes of Operation".
//
// THREAD-SAFETY: This class may is _not_ thread-safe. In particular, the class
// assumes that some other party ensures that QueueSdu() is not invoked
// concurrently with the destructor.
class BasicModeTxEngine final : public TxEngine {
 public:
  BasicModeTxEngine(ChannelId channel_id,
                    uint16_t max_tx_sdu_size,
                    SendFrameCallback send_frame_callback)
      : TxEngine(channel_id, max_tx_sdu_size, std::move(send_frame_callback)) {}
  ~BasicModeTxEngine() override = default;

  // Queues |sdu| for transmission, returning true on success. This may fail,
  // e.g., if the |sdu| is larger than |max_tx_sdu_size_|.
  bool QueueSdu(ByteBufferPtr sdu) override;

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BasicModeTxEngine);
};

}  // namespace bt::l2cap::internal
