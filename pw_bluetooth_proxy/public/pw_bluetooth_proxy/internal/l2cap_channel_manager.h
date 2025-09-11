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
#include <optional>

#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/h4_storage.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_status_tracker.h"
#include "pw_bluetooth_proxy/internal/locked_l2cap_channel.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_sync/lock_annotations.h"

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

  // Start proxying L2CAP packets addressed to `channel` arriving from
  // the controller and allow `channel` to send & queue Tx L2CAP packets.
  void RegisterChannel(L2capChannel& channel)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  // Stop proxying L2CAP packets addressed to `channel` and stop sending L2CAP
  // packets queued in `channel`, if `channel` is currently registered.
  void DeregisterChannel(L2capChannel& channel)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  // Deregister and close all channels then propagate `event` to clients.
  void DeregisterAndCloseChannels(L2capChannelEvent event)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  // Get an `H4PacketWithH4` backed by a buffer in `H4Storage` able to hold
  // `size` bytes of data.
  //
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  // Returns PW_STATUS_INVALID_ARGUMENT if `size` is too large for a buffer.
  pw::Result<H4PacketWithH4> GetAclH4Packet(uint16_t size);

  // Report that new tx packets have been queued or new tx credits have been
  // received since the last DrainChannelQueuesIfNewTx.
  void ReportNewTxPacketsOrCredits();

  // Send L2CAP packets queued in registered channels. Since this function takes
  // the channels_mutex_ lock, it can't be directly called while handling a
  // received packet on a channel. Instead call ReportPacketsMayBeReadyToSend().
  // Rx processing will then call this function when complete.
  void DrainChannelQueuesIfNewTx()
      PW_LOCKS_EXCLUDED(channels_mutex_, tx_status_mutex_);

  // Drain channel queues even if no channel explicitly requested it. Should be
  // used for events triggering queue space at the ACL level.
  void ForceDrainChannelQueues() PW_LOCKS_EXCLUDED(channels_mutex_);

  // Returns the size of an H4 buffer reserved for Tx packets.
  uint16_t GetH4BuffSize() const;

  std::optional<LockedL2capChannel> FindChannelByLocalCid(
      uint16_t connection_handle, uint16_t local_cid);

  std::optional<LockedL2capChannel> FindChannelByRemoteCid(
      uint16_t connection_handle, uint16_t remote_cid);

  // Must be called with channels_mutex_ held.
  L2capChannel* FindChannelByLocalCidLocked(uint16_t connection_handle,
                                            uint16_t local_cid);

  // Must be called with channels_mutex_ held.
  L2capChannel* FindChannelByRemoteCidLocked(uint16_t connection_handle,
                                             uint16_t remote_cid);

  // Register for notifications of connection and disconnection for a
  // particular L2cap service identified by its PSM.
  void RegisterStatusDelegate(L2capStatusDelegate& delegate);

  // Unregister a service delegate.
  void UnregisterStatusDelegate(L2capStatusDelegate& delegate);

  // Called when a l2cap channel connection successfully made.
  void HandleConnectionComplete(const L2capChannelConnectionInfo& info);

  // Called when a l2cap channel configuration is done.
  void HandleConfigurationChanged(const L2capChannelConfigurationInfo& info);

  // Called when an ACL connection is disconnected.
  void HandleAclDisconnectionComplete(uint16_t connection_handle);

  // Called when a l2cap channel connection is disconnected.
  //
  // Must be called under channels_mutex_ but we can't use proper lock
  // annotation here since the call comes via signaling channel.
  // TODO: https://pwbug.dev/390511432 - Figure out way to add annotations to
  // enforce this invariant.
  void HandleDisconnectionCompleteLocked(
      const L2capStatusTracker::DisconnectParams& params);

  // Deliver any pending connection events. Should not be called while holding
  // channels_mutex_.
  void DeliverPendingEvents();

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

  // Stop proxying L2CAP packets addressed to `channel` and stop sending L2CAP
  // packets queued in `channel`, if `channel` is currently registered.
  void DeregisterChannelLocked(L2capChannel& channel)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Reference to the ACL data channel owned by the proxy.
  AclDataChannel& acl_data_channel_;

  // Owns H4 packet buffers.
  H4Storage h4_storage_;

  std::atomic<std::optional<uint16_t>> le_acl_data_packet_length_{std::nullopt};

  // Enforce mutual exclusion of all operations on channels.
  // This is ACQUIRED_BEFORE AclDataChannel::credit_mutex_ which is annotated on
  // that member variable.
  sync::Mutex channels_mutex_;

  // List of registered L2CAP channels.
  IntrusiveForwardList<L2capChannel> channels_ PW_GUARDED_BY(channels_mutex_);

  // Iterator to "least recently drained" channel.
  IntrusiveForwardList<L2capChannel>::iterator lrd_channel_
      PW_GUARDED_BY(channels_mutex_);

  // Iterator to final channel to be visited in ongoing round robin.
  IntrusiveForwardList<L2capChannel>::iterator round_robin_terminus_
      PW_GUARDED_BY(channels_mutex_);

  // Guard access to tx related state.
  sync::Mutex tx_status_mutex_ PW_ACQUIRED_BEFORE(channels_mutex_);

  // True if new tx packets have been queued or new tx credits have been
  // received since the last DrainChannelQueuesIfNewTx.
  bool new_tx_since_drain_ PW_GUARDED_BY(tx_status_mutex_) = false;

  // Channel connection status tracker and delegate holder.
  L2capStatusTracker status_tracker_;
};

}  // namespace pw::bluetooth::proxy
