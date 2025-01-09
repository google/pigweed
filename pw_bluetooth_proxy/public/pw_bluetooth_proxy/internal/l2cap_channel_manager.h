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

#include <atomic>

#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/h4_storage.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_status_tracker.h"

namespace pw::bluetooth::proxy {

// `L2capChannelManager` mediates between `ProxyHost` and the L2CAP-based
// channels held by clients of `ProxyHost`, such as L2CAP connection-oriented
// channels, GATT Notify channels, and RFCOMM channels.
//
// ACL packet transmission is subject to data control flow, managed by
// `AclDataChannel`. `L2capChannelManager` handles queueing Tx packets when
// credits are unavailable and sending Tx packets as credits become available,
// dequeueing packets in FIFO order per channel and in round robin fashion
// around channels.
class L2capChannelManager {
 public:
  L2capChannelManager(AclDataChannel& acl_data_channel);

  // Reset state.
  void Reset();

  // Start proxying L2CAP packets addressed to `channel` arriving from
  // the controller and allow `channel` to send & queue Tx L2CAP packets.
  void RegisterChannel(L2capChannel& channel);

  // Stop proxying L2CAP packets addressed to `channel` and stop sending L2CAP
  // packets queued in `channel`, if `channel` is currently registered.
  void ReleaseChannel(L2capChannel& channel);

  // Get an `H4PacketWithH4` backed by a buffer in `H4Storage` able to hold
  // `size` bytes of data.
  //
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  // Returns PW_STATUS_INVALID_ARGUMENT if `size` is too large for a buffer.
  pw::Result<H4PacketWithH4> GetAclH4Packet(uint16_t size);

  // Send L2CAP packets queued in registered channels.
  void DrainChannelQueues() PW_LOCKS_EXCLUDED(channels_mutex_);

  // Returns the size of an H4 buffer reserved for Tx packets.
  uint16_t GetH4BuffSize() const;

  L2capChannel* FindChannelByLocalCid(uint16_t connection_handle,
                                      uint16_t local_cid);

  L2capChannel* FindChannelByRemoteCid(uint16_t connection_handle,
                                       uint16_t remote_cid);

  // Register for notifications of connection and disconnection for a
  // particular L2cap service identified by its PSM.
  void RegisterStatusDelegate(L2capStatusDelegate& delegate);

  // Unregister a service delegate.
  void UnregisterStatusDelegate(L2capStatusDelegate& delegate);

  // Called when a l2cap channel connection successfully made.
  void HandleConnectionComplete(const L2capChannelConnectionInfo& info);

  // Called when an ACL connection is disconnected.
  void HandleDisconnectionComplete(uint16_t connection_handle);

  // Called when a l2cap channel connection is disconnected.
  void HandleDisconnectionComplete(
      const L2capStatusTracker::DisconnectParams& params);

  // Core Spec v6.0 Vol 4, Part E, Section 7.8.2: "The LE_ACL_Data_Packet_Length
  // parameter shall be used to determine the maximum size of the L2CAP PDU
  // fragments that are contained in ACL data packets". A value of 0 means "No
  // dedicated LE Buffer exists".
  //
  // Return std::nullopt if HCI_LE_Read_Buffer_Size command complete event has
  // not yet been received.
  //
  // TODO: https://pwbug.dev/379339642 - Add tests to confirm this value caps
  // the size of Tx L2capCoc segments when segmentation is implemented.
  std::optional<uint16_t> le_acl_data_packet_length() const {
    return le_acl_data_packet_length_;
  }

  void set_le_acl_data_packet_length(uint16_t le_acl_data_packet_length) {
    le_acl_data_packet_length_ = le_acl_data_packet_length;
  }

 private:
  // Circularly advance `it`, wrapping around to front if `it` reaches the end.
  void Advance(IntrusiveForwardList<L2capChannel>::iterator& it)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Reference to the ACL data channel owned by the proxy.
  AclDataChannel& acl_data_channel_;

  // Owns H4 packet buffers.
  H4Storage h4_storage_;

  std::atomic<std::optional<uint16_t>> le_acl_data_packet_length_;

  // Enforce mutual exclusion of all operations on channels.
  sync::Mutex channels_mutex_;

  // List of registered L2CAP channels.
  IntrusiveForwardList<L2capChannel> channels_ PW_GUARDED_BY(channels_mutex_);

  // Iterator to "least recently drained" channel.
  IntrusiveForwardList<L2capChannel>::iterator lrd_channel_
      PW_GUARDED_BY(channels_mutex_);

  // Iterator to final channel to be visited in ongoing round robin.
  IntrusiveForwardList<L2capChannel>::iterator round_robin_terminus_
      PW_GUARDED_BY(channels_mutex_);

  // Channel connection status tracker and delegate holder.
  L2capStatusTracker status_tracker_;
};

}  // namespace pw::bluetooth::proxy
