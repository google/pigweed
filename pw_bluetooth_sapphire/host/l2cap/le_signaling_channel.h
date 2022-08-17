// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/le_connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Implements the L2CAP LE signaling fixed channel.
class LESignalingChannel final : public SignalingChannel {
 public:
  LESignalingChannel(fxl::WeakPtr<Channel> chan, hci_spec::ConnectionRole role);
  ~LESignalingChannel() override = default;

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannel);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
