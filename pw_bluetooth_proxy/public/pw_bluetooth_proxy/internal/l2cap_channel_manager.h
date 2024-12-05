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

#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/h4_storage.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"

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

  // Stop proxying L2CAP packets addressed to `channel`, stop sending L2CAP
  // packets queued in `channel`, and clear its queue.
  //
  // Returns false if `channel` is not found.
  bool ReleaseChannel(L2capChannel& channel);

  // Get an `H4PacketWithH4` backed by a buffer in `H4Storage` able to hold
  // `size` bytes of data.
  //
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  // Returns PW_STATUS_INVALID_ARGUMENT if `size` is too large for a buffer.
  pw::Result<H4PacketWithH4> GetTxH4Packet(uint16_t size);

  // Send L2CAP packets queued in registered channels.
  void DrainChannelQueues() PW_LOCKS_EXCLUDED(channels_mutex_);

  // Returns the size of an H4 buffer reserved for Tx packets.
  uint16_t GetH4BuffSize() const;

  L2capChannel* FindChannelByLocalCid(uint16_t connection_handle,
                                      uint16_t local_cid);

  L2capChannel* FindChannelByRemoteCid(uint16_t connection_handle,
                                       uint16_t remote_cid);

 private:
  // Circularly advance `it`, wrapping around to front if `it` reaches the end.
  void Advance(IntrusiveForwardList<L2capChannel>::iterator& it)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Send L2CAP packets queued in registered channels as long as ACL credits are
  // available on the specified transport.
  void DrainChannelQueues(AclTransportType transport)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Reference to the ACL data channel owned by the proxy.
  AclDataChannel& acl_data_channel_;

  // Owns H4 packet buffers.
  H4Storage h4_storage_;

  // Enforce mutual exclusion of all operations on channels.
  sync::Mutex channels_mutex_;

  // List of registered L2CAP channels.
  IntrusiveForwardList<L2capChannel> channels_ PW_GUARDED_BY(channels_mutex_);

  // Iterator to "least recently drained" channel.
  IntrusiveForwardList<L2capChannel>::iterator lrd_channel_
      PW_GUARDED_BY(channels_mutex_);
};

}  // namespace pw::bluetooth::proxy
