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

#include "pw_bluetooth_proxy/common.h"

namespace pw::bluetooth::proxy {

/// `ProxyPolicy` subclasses provide proxy functionality within a
/// `pw::bluetooth::proxy::HciProxy`. They should be instantiated and passed to
/// the proxy when it is constructed.
class ProxyPolicy {
 public:
  ProxyPolicy() = default;
  virtual ~ProxyPolicy() = default;
  ProxyPolicy(const ProxyPolicy&) = delete;
  ProxyPolicy& operator=(const ProxyPolicy&) = delete;
  ProxyPolicy(ProxyPolicy&&) = delete;
  ProxyPolicy& operator=(ProxyPolicy&&) = delete;

  // Proxy APIs

  /// Process H4 HCI packet sent from the host side towards the controller side.
  void virtual ProcessH4HciFromHost(H4HciPacket packet) = 0;

  /// Process HCI packet sent from the controller side towards the host side.
  void virtual ProcessH4HciFromController(H4HciPacket packet) = 0;

  /// Sets the callback that will be called by the policy when it wants to send
  /// HCI bytes towards the host.
  void setSendToHostFn(H4HciPacketSendFn&& send_to_host_fn) {
    send_to_host_fn_ = std::move(send_to_host_fn);
  }

  // Sets the callback that will be called by the policy when it wants to send
  // HCI bytes towards the controller.
  void setSendToControllerFn(H4HciPacketSendFn&& send_to_controller_fn) {
    send_to_controller_fn_ = std::move(send_to_controller_fn);
  }

 protected:
  // See `setSendToHostFn`.
  H4HciPacketSendFn send_to_host_fn_;

  // See `setSendToControllerFn`.
  H4HciPacketSendFn send_to_controller_fn_;
};

}  // namespace pw::bluetooth::proxy
