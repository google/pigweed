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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_

#include <memory>
#include <utility>

#include "pw_bluetooth_sapphire/internal/host/l2cap/enhanced_retransmission_mode_rx_engine.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/enhanced_retransmission_mode_tx_engine.h"

namespace bt::l2cap::internal {

// Construct a pair of EnhancedRetransmissionMode{Rx,Tx}Engines that are
// synchronized to each other and share a |send_frame_callback|. They must be
// run on the same thread and can be deleted in any order, but must be deleted
// consecutively without calling their methods in between.
//
// The parameters are identical to the EnhancedRetransmissionTxEngine ctor.
std::pair<std::unique_ptr<EnhancedRetransmissionModeRxEngine>,
          std::unique_ptr<EnhancedRetransmissionModeTxEngine>>
MakeLinkedEnhancedRetransmissionModeEngines(
    ChannelId channel_id,
    uint16_t max_tx_sdu_size,
    uint8_t max_transmissions,
    uint8_t n_frames_in_tx_window,
    EnhancedRetransmissionModeTxEngine::SendFrameCallback send_frame_callback,
    EnhancedRetransmissionModeTxEngine::ConnectionFailureCallback
        connection_failure_callback,
    pw::async::Dispatcher& dispatcher);

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_
