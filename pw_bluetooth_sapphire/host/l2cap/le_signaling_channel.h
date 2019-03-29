// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"
#include "src/lib/fxl/macros.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements the L2CAP LE signaling fixed channel.
class LESignalingChannel final : public SignalingChannel {
 public:
  using ConnectionParameterUpdateCallback =
      fit::function<void(const hci::LEPreferredConnectionParameters& params)>;

  LESignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role);
  ~LESignalingChannel() override = default;

  // SignalingChannelInterface overrides
  bool SendRequest(CommandCode req_code, const common::ByteBuffer& payload,
                   ResponseHandler cb) override;
  void ServeRequest(CommandCode req_code, RequestDelegate cb) override;

  // Sets a |callback| to be invoked when a Connection Parameter Update request
  // is received with the given parameters. LESignalingChannel will
  // automatically accept these parameters, however it is up to the
  // implementation of |callback| to apply them to the controller.
  //
  // This task will be posted onto the given |dispatcher|.
  void set_conn_param_update_callback(
      ConnectionParameterUpdateCallback callback,
      async_dispatcher_t* dispatcher) {
    ZX_DEBUG_ASSERT(IsCreationThreadCurrent());
    ZX_DEBUG_ASSERT(static_cast<bool>(callback) ==
                    static_cast<bool>(dispatcher));
    conn_param_update_cb_ = std::move(callback);
    dispatcher_ = dispatcher;
  }

 private:
  void OnConnParamUpdateReceived(const SignalingPacket& packet);

  // SignalingChannel override
  void DecodeRxUnit(common::ByteBufferPtr sdu,
                    const SignalingPacketHandler& cb) override;

  bool HandlePacket(const SignalingPacket& packet) override;

  ConnectionParameterUpdateCallback conn_param_update_cb_;
  async_dispatcher_t* dispatcher_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LESignalingChannel);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LE_SIGNALING_CHANNEL_H_
