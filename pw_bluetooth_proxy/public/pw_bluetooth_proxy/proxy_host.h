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

#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth_proxy/acl_data_channel.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

/// `ProxyHost` acts as the main coordinator for proxy functionality. After
/// creation, the container then passes packets through the proxy.
class ProxyHost {
 public:
  /// Creates an `ProxyHost` that will process HCI packets.
  /// @param[in] send_to_host_fn Callback that will be called when proxy wants
  /// to send HCI packet towards the host.
  /// @param[in] send_to_controller_fn - Callback that will be called when
  /// proxy wants to send HCI packet towards the controller.
  ProxyHost(pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
            pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
            uint16_t le_acl_credits_to_reserve);

  ProxyHost() = delete;
  virtual ~ProxyHost() = default;
  ProxyHost(const ProxyHost&) = delete;
  ProxyHost& operator=(const ProxyHost&) = delete;
  ProxyHost(ProxyHost&&) = delete;
  ProxyHost& operator=(ProxyHost&&) = delete;

  // ##### Container API
  // Containers are expected to call these functions (in addition to ctor).

  /// Called by container to ask proxy to handle a H4 HCI packet sent from the
  /// host side towards the controller side. Proxy will in turn call the
  /// `send_to_controller_fn` provided during construction to pass the packet
  /// on to the controller. Some packets may be modified, added, or removed.
  ///
  /// The proxy host currently does not require any from-host packets to support
  /// its current functionality. It will pass on all packets, so containers can
  /// choose to just pass all from-host packets through it.
  void HandleH4HciFromHost(H4PacketWithH4&& h4_packet);

  /// Called by container to ask proxy to handle a H4 packet sent from the
  /// controller side towards the host side. Proxy will in turn call the
  /// `send_to_host_fn` provided during construction to pass the packet on to
  /// the host. Some packets may be modified, added, or removed.
  ///
  /// To support all of its current functionality, the proxy host needs at least
  /// the following from-controller events passed through it. It will pass on
  /// all other packets, so containers can choose to just pass all
  /// from-controller packets through it.
  /// - 7.7.14 Command Complete event (7.8.2 LE Read Buffer Size [v1] command)
  /// - 7.7.14 Command Complete event (7.8.2 LE Read Buffer Size [v2] command)
  /// - 7.7.19 Number Of Completed Packets event (v1.1)
  /// - 7.7.5 Disconnection Complete event (v1.1)
  void HandleH4HciFromController(H4PacketWithHci&& h4_packet);

  /// Called by container to notify proxy that the Bluetooth system is being
  /// reset, so the proxy can reset its internal state.
  /// Warning: Outstanding H4 packets are not invalidated upon reset. If they
  /// are destructed post-reset, packets generated post-reset are liable to be
  /// overwritten prematurely.
  void Reset();

  // ##### Client APIs

  /// Send a GATT Notify to the indicated connection.
  ///
  /// @param[in] connection_handle The connection handle of the peer to notify
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
  pw::Status sendGattNotify(uint16_t connection_handle,
                            uint16_t attribute_handle,
                            pw::span<const uint8_t> attribute_value);

  /// Indicates whether the proxy has the capability of sending ACL packets.
  /// Note that this indicates intention, so it can be true even if the proxy
  /// has not yet or has been unable to reserve credits from the host.
  bool HasSendAclCapability() const;

  /// Returns the number of available LE ACL send credits for the proxy.
  /// Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeLeAclPackets() const;

 private:
  // Process/update the packet.
  void ProcessH4HciFromController(pw::span<uint8_t> hci_buffer);

  // Populate the fields of the provided ATT_HANDLE_VALUE_NTF packet view.
  void BuildAttNotify(emboss::AttNotifyOverAclWriter att_notify,
                      uint16_t connection_handle,
                      uint16_t attribute_handle,
                      pw::span<const uint8_t> attribute_value)
      PW_EXCLUSIVE_LOCKS_REQUIRED(acl_send_mutex_);

  // Process a Command_Complete event.
  void ProcessCommandCompleteEvent(pw::span<uint8_t> hci_buffer);

  // For sending non-ACL data to the host and controller. ACL traffic shall be
  // sent through the `acl_data_channel_`.
  HciTransport hci_transport_;

  // Owns management of the LE ACL data channel.
  AclDataChannel acl_data_channel_;

  // Max size of `h4_buff_`.
  // TODO: https://pwbug.dev/349700888 - Make size configurable.
  static constexpr uint16_t kH4BuffSize = 14;

  // Sending & releasing ACL packets happen on different threads. As such, we
  // need a mutex to guard around all operations in the ACL-send pipeline,
  // including building packets, credit allocation, and releasing packets.
  sync::Mutex acl_send_mutex_;

  // Static buffer to hold one H4 packet containing an ACL PDU.
  std::array<uint8_t, kH4BuffSize> h4_buff_ PW_GUARDED_BY(acl_send_mutex_);

  // Set when an H4 packet is sent through the `acl_data_channel_` and cleared
  // in that H4 packet's release function to indicate that `h4_buff_` is safe to
  // overwrite.
  bool acl_send_pending_ PW_GUARDED_BY(acl_send_mutex_);
};

}  // namespace pw::bluetooth::proxy
