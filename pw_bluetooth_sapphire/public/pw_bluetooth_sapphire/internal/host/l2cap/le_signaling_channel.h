// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/le_connection_parameters.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Implements the L2CAP LE signaling fixed channel.
class LESignalingChannel final : public SignalingChannel {
 public:
  LESignalingChannel(Channel::WeakPtr chan,
                     pw::bluetooth::emboss::ConnectionRole role,
                     pw::async::Dispatcher& dispatcher);
  ~LESignalingChannel() override = default;

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu,
                    const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannel);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
