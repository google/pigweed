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

#include "pw_allocator/best_fit.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_bluetooth_proxy/gatt_notify_channel.h"
#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/h4_storage.h"
#include "pw_bluetooth_proxy/internal/hci_transport.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_bluetooth_proxy/l2cap_coc.h"
#include "pw_bluetooth_proxy/l2cap_status_delegate.h"
#include "pw_bluetooth_proxy/rfcomm_channel.h"
#include "pw_multibuf/simple_allocator.h"
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
  /// @param[in] le_acl_credits_to_reserve - How many buffers to reserve for the
  /// proxy out of any LE ACL buffers received from controller.
  /// @param[in] br_edr_acl_credits_to_reserve - How many buffers to reserve for
  /// the proxy out of any BR/EDR ACL buffers received from controller.
  ProxyHost(pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
            pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
            uint16_t le_acl_credits_to_reserve,
            uint16_t br_edr_acl_credits_to_reserve);

  ProxyHost() = delete;
  ProxyHost(const ProxyHost&) = delete;
  ProxyHost& operator=(const ProxyHost&) = delete;
  ProxyHost(ProxyHost&&) = delete;
  ProxyHost& operator=(ProxyHost&&) = delete;
  /// Deregisters all channels, and if any channels are not yet closed, closes
  /// them and sends `L2capChannelEvent::kChannelClosedByOther`.
  ~ProxyHost();

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
  ///
  /// Container is required to call this function synchronously (one packet at a
  /// time).
  void HandleH4HciFromHost(H4PacketWithH4&& h4_packet);

  /// Called by container to ask proxy to handle a H4 packet sent from the
  /// controller side towards the host side. Proxy will in turn call the
  /// `send_to_host_fn` provided during construction to pass the packet on to
  /// the host. Some packets may be modified, added, or removed.
  ///
  /// To support all of its current functionality, the proxy host needs at least
  /// the following from-controller packets passed through it. It will pass on
  /// all other packets, so containers can choose to just pass all
  /// from-controller packets through the proxy host.
  ///
  /// All packets of this type:
  /// - L2CAP over ACL packets (specifically those addressed to channels managed
  ///   by the proxy host, including signaling packets)
  ///
  /// HCI_Command_Complete events (7.7.14) containing return parameters for
  /// these commands:
  /// - HCI_LE_Read_Buffer_Size [v1] command (7.8.2)
  /// - HCI_LE_Read_Buffer_Size [v2] command (7.8.2)
  ///
  /// These HCI event packets:
  /// - HCI_Number_Of_Completed_Packets event (7.7.19)
  /// - HCI_Disconnection_Complete event (7.7.5)
  ///
  /// Container is required to call this function synchronously (one packet at a
  /// time).
  void HandleH4HciFromController(H4PacketWithHci&& h4_packet);

  /// Called when an HCI_Reset Command packet is observed. Proxy resets its
  /// internal state. Deregisters all channels, and if any channels are not yet
  /// closed, closes them and sends `L2capChannelEvent::kReset`.
  ///
  /// May also be called by container to notify proxy that the Bluetooth system
  /// is being otherwise reset.
  ///
  /// Warning: Outstanding H4 packets are not invalidated upon reset. If they
  /// are destructed post-reset, packets generated post-reset are liable to be
  /// overwritten prematurely.
  void Reset();

  // ##### Client APIs

  /// Register for notifications of connection and disconnection for a
  /// particular L2cap service identified by its PSM.
  ///
  /// @param[in] delegate   A delegate that will be notified when a successful
  ///                       L2cap connection is made on its PSM. Note: This
  ///                       must outlive the ProxyHost.
  void RegisterL2capStatusDelegate(L2capStatusDelegate& delegate);

  /// Unregister a service delegate.
  ///
  /// @param[in] delegate   The delegate to unregister. Must have been
  ///                       previously registered.
  void UnregisterL2capStatusDelegate(L2capStatusDelegate& delegate);

  /// Returns an L2CAP connection-oriented channel that supports writing to and
  /// reading from a remote peer.
  ///
  /// @param[in] rx_multibuf_allocator
  ///                               Provides the allocator the channel will use
  ///                               for its Rx buffers (for both queueing and
  ///                               returning to the client).
  ///
  /// @param[in] connection_handle  The connection handle of the remote peer.
  ///
  /// @param[in] rx_config          Parameters applying to reading packets. See
  ///                               `l2cap_coc.h` for details.
  ///
  /// @param[in] tx_config          Parameters applying to writing packets. See
  ///                               `l2cap_coc.h` for details.
  ///
  /// @param[in] receive_fn         Read callback to be invoked on Rx SDUs.
  ///
  /// @param[in] event_fn          Handle asynchronous events such as errors and
  ///                              flow control events encountered by the
  ///                              channel. See `l2cap_channel_event.h`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  ///  UNAVAILABLE:      If channel could not be created because no memory was
  ///                    available to accommodate an additional ACL connection.
  /// @endrst
  pw::Result<L2capCoc> AcquireL2capCoc(
      pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
      uint16_t connection_handle,
      L2capCoc::CocConfig rx_config,
      L2capCoc::CocConfig tx_config,
      Function<void(multibuf::MultiBuf&& payload)>&& receive_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  /// TODO: https://pwbug.dev/380076024 - Delete after downstream client uses
  /// this method on `L2capCoc`.
  /// @deprecated Use L2capCoc::SendAdditionalRxCredits instead.
  pw::Status SendAdditionalRxCredits(uint16_t connection_handle,
                                     uint16_t local_cid,
                                     uint16_t additional_rx_credits);

  /// Returns an L2CAP channel operating in basic mode that supports writing to
  /// and reading from a remote peer.
  ///
  /// @param[in] rx_multibuf_allocator      Provides the allocator the channel
  ///                                       will use for its Rx buffers (for
  ///                                       both queueing and  returning to the
  ///                                       client).
  ///
  /// @param[in] connection_handle          The connection handle of the remote
  ///                                       peer.
  ///
  /// @param[in] local_cid                  L2CAP channel ID of the local
  ///                                       endpoint.
  ///
  /// @param[in] remote_cid                 L2CAP channel ID of the remote
  ///                                       endpoint.
  ///
  /// @param[in] transport                  Logical link transport type.
  ///
  /// @param[in] payload_from_controller_fn Read callback to be invoked on Rx
  ///                                       SDUs. Return value of passed-in
  ///                                       multibuf indicates the packet should
  ///                                       be forwarded on to host.
  ///
  /// @param[in] payload_from_host_fn       Read callback to be invoked on Tx
  ///                                       SDUs. Return value of passed-in
  ///                                       multibuf indicates the packet should
  ///                                       be forwarded on to the controller.
  ///
  /// @param[in] event_fn                   Handle asynchronous events such as
  ///                                       errors encountered by the channel.
  ///                                       See `l2cap_channel_common.h`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  ///  UNAVAILABLE:      If channel could not be created because no memory was
  ///                    available to accommodate an additional ACL connection.
  /// @endrst
  pw::Result<BasicL2capChannel> AcquireBasicL2capChannel(
      multibuf::MultiBufAllocator& rx_multibuf_allocator,
      uint16_t connection_handle,
      uint16_t local_cid,
      uint16_t remote_cid,
      AclTransportType transport,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  /// Returns a GATT Notify channel channel that supports sending notifications
  /// to a particular connection handle and attribute.
  ///
  /// @param[in] connection_handle The connection handle of the peer to notify.
  /// Maximum valid connection handle is 0x0EFF.
  ///
  /// @param[in] attribute_handle  The attribute handle the notify should be
  /// sent on. Cannot be 0.
  ///
  /// @param[in] event_fn          Handle asynchronous events such as errors and
  ///                              flow control events encountered by the
  ///                              channel. See `l2cap_channel_event.h`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  ///  UNAVAILABLE:      If channel could not be created because no memory was
  ///                    available to accommodate an additional ACL connection.
  /// @endrst
  pw::Result<GattNotifyChannel> AcquireGattNotifyChannel(
      int16_t connection_handle,
      uint16_t attribute_handle,
      Function<void(L2capChannelEvent event)>&& event_fn);

  /// Send a GATT Notify to the indicated connection.
  ///
  /// @param[in] connection_handle The connection handle of the peer to notify.
  /// Maximum valid connection handle is 0x0EFF.
  ///
  /// @param[in] attribute_handle  The attribute handle the notify should be
  /// sent on. Cannot be 0.
  /// @param[in] attribute_value   The client payload to be sent. Payload will
  /// be destroyed once its data has been used.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK: If notify was successfully queued for send.
  ///  UNAVAILABLE: If CHRE doesn't have resources to queue the send at this
  ///  time (transient error).
  ///
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  /// @endrst
  ///
  /// @deprecated - Clients should use `ProxyHost::AcquireGattNotifyChannel` and
  /// then call `GattNotifyChannel::Write` on that.
  // TODO: https://pwbug.dev/369709521 - Delete this once all downstreams
  // have transitioned.
  StatusWithMultiBuf SendGattNotify(uint16_t connection_handle,
                                    uint16_t attribute_handle,
                                    pw::multibuf::MultiBuf&& payload);

  /// Send a GATT Notify to the indicated connection.
  ///
  /// Deprecated, use MultiBuf version above instead.
  ///
  /// @param[in] connection_handle The connection handle of the peer to notify.
  ///                              Maximum valid connection handle is 0x0EFF.
  ///
  /// @param[in] attribute_handle  The attribute handle the notify should be
  ///                              sent on. Cannot be 0.
  /// @param[in] attribute_value   The data to be sent. Data will be copied
  ///                              before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK: If notify was successfully queued for send.
  ///  UNAVAILABLE: If CHRE doesn't have resources to queue the send
  ///               at this time (transient error).
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  /// @endrst
  /// @deprecated - Clients should use `ProxyHost::AcquireGattNotifyChannel` and
  /// then call `GattNotifyChannel::Write` on that.
  // TODO: https://pwbug.dev/379337272 - Delete this once all downstreams
  // have transitioned.
  pw::Status SendGattNotify(uint16_t connection_handle,
                            uint16_t attribute_handle,
                            pw::span<const uint8_t> attribute_value);

  /// Returns an RFCOMM channel that supports writing to and reading from a
  /// remote peer.
  ///
  /// @param[in] rx_multibuf_allocator
  ///                              Provides the allocator the channel will use
  ///                              for its Rx buffers (for both queueing and
  ///                              returning to the client).
  ///
  /// @param[in] connection_handle The connection handle of the remote peer.
  ///
  /// @param[in] rx_config         Parameters applying to reading packets.
  ///                              See `rfcomm_channel.h` for details.
  ///
  /// @param[in] tx_config         Parameters applying to writing packets.
  ///                              See `rfcomm_channel.h` for details.
  ///
  /// @param[in] channel_number    RFCOMM channel number to use.
  ///
  /// @param[in] payload_from_controller_fn
  ///                              Read callback to be invoked on Rx frames.
  ///
  /// @param[in] event_fn          Handle asynchronous events such as errors
  ///                              encountered by the channel. See
  ///                              `l2cap_channel_common.h`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  ///  UNAVAILABLE: If channel could not be created.
  /// @endrst
  pw::Result<RfcommChannel> AcquireRfcommChannel(
      multibuf::MultiBufAllocator& rx_multibuf_allocator,
      uint16_t connection_handle,
      RfcommChannel::Config rx_config,
      RfcommChannel::Config tx_config,
      uint8_t channel_number,
      Function<void(multibuf::MultiBuf&& payload)>&& payload_from_controller_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  /// Indicates whether the proxy has the capability of sending LE ACL packets.
  /// Note that this indicates intention, so it can be true even if the proxy
  /// has not yet or has been unable to reserve credits from the host.
  bool HasSendLeAclCapability() const;

  /// Indicates whether the proxy has the capability of sending BR/EDR ACL
  /// packets. Note that this indicates intention, so it can be true even if the
  /// proxy has not yet or has been unable to reserve credits from the host.
  bool HasSendBrEdrAclCapability() const;

  /// Returns the number of available LE ACL send credits for the proxy.
  /// Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeLeAclPackets() const;

  /// Returns the number of available BR/EDR ACL send credits for the proxy.
  /// Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeBrEdrAclPackets() const;

  /// Returns the max number of LE ACL sends that can be in-flight at one time.
  /// That is, ACL packets that have been sent and not yet released.
  static constexpr size_t GetNumSimultaneousAclSendsSupported() {
    return H4Storage::GetNumH4Buffs();
  }

  /// Returns the max LE ACL packet size supported to be sent.
  static constexpr size_t GetMaxAclSendSize() {
    return H4Storage::GetH4BuffSize() - sizeof(emboss::H4PacketType);
  }

  /// Returns the max number of simultaneous LE ACL connections supported.
  static constexpr size_t GetMaxNumAclConnections() {
    return AclDataChannel::GetMaxNumAclConnections();
  }

 private:
  // Handle HCI Event packet from the controller.
  void HandleEventFromController(H4PacketWithHci&& h4_packet);

  // Handle HCI Event packet from the host.
  void HandleEventFromHost(H4PacketWithH4&& h4_packet);

  // Handle HCI ACL data packet from the controller.
  void HandleAclFromController(H4PacketWithHci&& h4_packet);

  // Process an LE_META_EVENT
  void HandleLeMetaEvent(H4PacketWithHci&& h4_packet);

  // Process a Command_Complete event.
  void HandleCommandCompleteEvent(H4PacketWithHci&& h4_packet);

  // Handle HCI Command packet from the host.
  void HandleCommandFromHost(H4PacketWithH4&& h4_packet);

  // Handle HCI ACL data packet from the host.
  void HandleAclFromHost(H4PacketWithH4&& h4_packet);

  // For sending non-ACL data to the host and controller. ACL traffic shall be
  // sent through the `acl_data_channel_`.
  HciTransport hci_transport_;

  // Owns management of the LE ACL data channel.
  AclDataChannel acl_data_channel_;

  // Keeps track of the L2CAP-based channels managed by the proxy.
  L2capChannelManager l2cap_channel_manager_;
};

}  // namespace pw::bluetooth::proxy
