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

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth_proxy/common.h"
#include "pw_bluetooth_proxy/proxy_policy.h"

namespace pw::bluetooth::proxy {

/// `HciProxy` acts as the main coordinator for proxy functionality. A container
/// should instantiate it with `pw::bluetooth::proxy::ProxyPolicy` instances
/// that provide the proxy behavior the container wants. The container then
/// passes packets through the proxy.
class HciProxy {
 public:
  /// Creates an `HciProxy` that will process HCI packets using the passed
  /// policies.
  /// @param[in] send_to_host_fn Callback that will be called when proxy wants
  /// to send HCI packet towards the host.
  /// @param[in] send_to_controller_fn - Callback that will be called when proxy
  /// wants to send HCI packet towards the controller.
  /// @param[in] policies - Policies that will be applied to HCI packets.
  /// Policies are applied in forward order for packets from host to controller,
  /// and in the reverse order for packets from controller to host. Proxy does
  /// not take ownership of content, span and policies must outlive this object.
  HciProxy(H4HciPacketSendFn&& send_to_host_fn,
           H4HciPacketSendFn&& send_to_controller_fn,
           pw::span<ProxyPolicy*> policies);

  HciProxy() = delete;
  virtual ~HciProxy() = default;
  HciProxy(const HciProxy&) = delete;
  HciProxy& operator=(const HciProxy&) = delete;
  HciProxy(HciProxy&&) = delete;
  HciProxy& operator=(HciProxy&&) = delete;

  // Container APIs

  /// Called by container to ask proxy to process an H4 HCI packet sent from the
  /// host side towards the controller side. Proxy will in turn call the
  /// `send_to_controller_fn` provided during construction to pass the packet on
  /// to the controller. Depending on the policies, some packets may be
  /// modified, added, or removed.
  void virtual ProcessH4HciFromHost(H4HciPacket packet) {
    first_process_h4_hci_from_host_fn_(packet);
  }

  /// Called by container to ask proxy to process HCI packet sent from the
  /// controller side towards the host side. Proxy will in turn call the
  /// `send_to_host_fn` provided during construction to pass the packet on to
  /// the host. Depending on the policies, some packets may be modified, added,
  /// or removed.
  void virtual ProcessH4HciFromController(H4HciPacket packet) {
    first_process_h4_hci_from_controller_fn_(packet);
  }

 private:
  void sendToHost(H4HciPacket packet) {
    PW_DCHECK(outward_send_to_host_fn_ != nullptr);
    outward_send_to_host_fn_(packet);
  }

  void sendToController(H4HciPacket packet) {
    PW_DCHECK(outward_send_to_controller_fn_ != nullptr);
    outward_send_to_controller_fn_(packet);
  }

  // Sets the first function to call when processing a packet coming from the
  // host. Generally this will be the first policy's
  // `pw::bluetooth::proxy::ProxyPolicy::ProcessH4HciFromHost`. If there are no
  // policies this this will just be `this.sendToController`.
  void setFirstProcessH4HciFromHostFn(H4HciPacketSendFn&& fn) {
    first_process_h4_hci_from_host_fn_ = std::move(fn);
  }

  // Sets first function to call when processing a packet coming from the
  // controller. Generally this will be the last policy's
  // `pw::bluetooth::proxy::ProxyPolicy::ProcessH4HciFromController` since
  // policies are called in reverse order for controller-to-host packets. If
  // there are no policies this will just be `this.sendToHost`.
  void setFirstProcessH4HciFromControllerFn(H4HciPacketSendFn&& fn) {
    first_process_h4_hci_from_controller_fn_ = std::move(fn);
  }

  // See `setFirstProcessH4HciFromHostFn`.
  H4HciPacketSendFn first_process_h4_hci_from_host_fn_;

  // See `setFirstProcessH4HciFromControllerFn`.
  H4HciPacketSendFn first_process_h4_hci_from_controller_fn_;

  // Function to call when proxy wants proxy container to pass a packet to the
  // host.
  H4HciPacketSendFn outward_send_to_host_fn_;

  // Function to call when proxy wants proxy container to pass a packet to the
  // controller.
  H4HciPacketSendFn outward_send_to_controller_fn_;

  // List of policies. The proxy does not own the span or the policies
  // themselves.
  pw::span<ProxyPolicy*> policies_;
};

}  // namespace pw::bluetooth::proxy
