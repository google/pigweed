// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_

#include <zircon/assert.h>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements the L2CAP LE signaling fixed channel.
class LESignalingChannel final : public SignalingChannel {
 public:
  LESignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role);
  ~LESignalingChannel() override = default;

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannel);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
