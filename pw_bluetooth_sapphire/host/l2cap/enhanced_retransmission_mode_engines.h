// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_

#include <memory>
#include <utility>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_rx_engine.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_tx_engine.h"

namespace bt::l2cap::internal {

// Construct a pair of EnhancedRetransmissionMode{Rx,Tx}Engines that are synchronized to each other
// and share a |send_frame_callback|. They must be run on the same thread and can be deleted in any
// order, but must be deleted consecutively without calling their methods in between.
//
// The parameters are identical to the EnhancedRetransmissionTxEngine ctor.
std::pair<std::unique_ptr<EnhancedRetransmissionModeRxEngine>,
          std::unique_ptr<EnhancedRetransmissionModeTxEngine>>
MakeLinkedEnhancedRetransmissionModeEngines(
    ChannelId channel_id, uint16_t max_tx_sdu_size, uint8_t max_transmissions,
    uint8_t n_frames_in_tx_window,
    EnhancedRetransmissionModeTxEngine::SendFrameCallback send_frame_callback,
    EnhancedRetransmissionModeTxEngine::ConnectionFailureCallback connection_failure_callback);

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_ENHANCED_RETRANSMISSION_MODE_ENGINES_H_
