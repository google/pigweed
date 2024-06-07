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
#include "pw_bluetooth_proxy/acl_data_channel.h"
#include "pw_bluetooth_proxy/common.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

/// `ProxyHost` acts as the main coordinator for proxy functionality. After
/// creation, the container then passes packets through the proxy.
class ProxyHost {
 public:
  /// Creates an `ProxyHost` that will process HCI packets.
  /// @param[in] send_to_host_fn Callback that will be called when proxy wants
  /// to send HCI packet towards the host.
  /// @param[in] send_to_controller_fn - Callback that will be called when proxy
  /// wants to send HCI packet towards the controller.
  ProxyHost(H4HciPacketSendFn&& send_to_host_fn,
            H4HciPacketSendFn&& send_to_controller_fn,
            uint16_t le_acl_credits_to_reserve);

  ProxyHost() = delete;
  virtual ~ProxyHost() = default;
  ProxyHost(const ProxyHost&) = delete;
  ProxyHost& operator=(const ProxyHost&) = delete;
  ProxyHost(ProxyHost&&) = delete;
  ProxyHost& operator=(ProxyHost&&) = delete;

  // ##### Container APIs

  /// Called by container to ask proxy to handle a H4 HCI packet sent from the
  /// host side towards the controller side. Proxy will in turn call the
  /// `send_to_controller_fn` provided during construction to pass the packet on
  /// to the controller. Some packets may be modified, added, or removed.
  void HandleH4HciFromHost(H4HciPacket h4_packet);

  /// Called by container to ask proxy to handle a H4 packet sent from the
  /// controller side towards the host side. Proxy will in turn call the
  /// `send_to_host_fn` provided during construction to pass the packet on to
  /// the host. Some packets may be modified, added, or removed.
  void HandleH4HciFromController(H4HciPacket h4_packet);

  // ##### Client APIs

  /// Send a GATT Notify to the indicated connection.
  ///
  /// @param[in] connection_handle The connection handle of the peer the notify
  ///                              is being sent to.
  ///
  /// @param[in] attribute_handle  The attribute handle the notify should be
  ///                              sent on.
  /// @param[in] attribute_value   The data to be sent. Data will be copied
  ///                              before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK: If notify was successfully queued for send.
  ///  UNAVAILABLE: If CHRE doesn't have resources to queue the send
  ///               at this time (transient error).
  ///  UNIMPLEMENTED: If send is not supported by the current implementation.
  ///  INVALID_ARGUMENT: If arguments are invalid.
  /// @endrst
  pw::Status sendGattNotify(
      [[maybe_unused]] uint16_t connection_handle,
      [[maybe_unused]] uint16_t attribute_handle,
      [[maybe_unused]] pw::span<const uint8_t> attribute_value) {
    return pw::Status::Unimplemented();
  }

  /// Indicates whether the proxy has the capability of sending ACL packets.
  /// Note that this indicates intention, so it can be true even if the proxy
  /// has not yet or has been unable to reserve credits from the host.
  bool HasSendAclCapability() const;

  /// Returns the number of available LE ACL send credits for the proxy.
  /// Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeLeAclPackets() const;

 private:
  // Process/update the packet.
  void ProcessH4HciFromController(H4HciPacket h4_packet);

  // Send packet onwards to host.
  void SendToHost(H4HciPacket h4_packet);

  // Send packet onwards to controller.
  void SendToController(H4HciPacket h4_packet);

  // Function to call when proxy wants proxy container to pass a packet to the
  // host.
  H4HciPacketSendFn outward_send_to_host_fn_;

  // Function to call when proxy wants proxy container to pass a packet to the
  // controller.
  H4HciPacketSendFn outward_send_to_controller_fn_;

  // Owns management of the HCI LE ACL data channel.
  AclDataChannel acl_data_channel_;
};

}  // namespace pw::bluetooth::proxy
