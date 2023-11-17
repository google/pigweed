// Copyright 2023 The Pigweed Authors
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
#include <lib/fit/function.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"

namespace bt::testing {

// Manages individual FakeDynamicChannel instances as created by
// FakeSignalingServer. There are two potential states that an indiviudal
// channel can be in - open and closed.
// If the channel is open, then device has received a ConnectionRequest
// associated with this channel, sent out a corresponding ConfigurationRequest,
// and received a ConfigurationRequest in response. The channel is ready to
// handle packets.
// Closed: The channel is not ready to handle packets. It may still be
// registered with the FakeL2cap instance managing it.
// Note that when the device has received a ConnectionRequest and sent out a
// ConfigurationRequest but is still awaiting a ConfigurationRequest from
// bt-host, it will still be connected (as there will be a local channel ID
// assigned to it) but it is not Open state.
class FakeDynamicChannel {
 public:
  // Callback associated with handling packet |sdu| sent across the channel.
  // Set by the service associated with the channel's PSM.
  using PacketHandlerCallback = fit::function<void(const ByteBuffer& sdu)>;

  // Callback associated with sending packet |sdu| through this dynamic
  // channel. Set by the service associated with the channel's PSM.
  using SendPacketCallback = fit::function<void(const ByteBuffer& sdu)>;

  // Callback associated with closing and tearing down this dynamic channel.
  // Set by the service associated with the channel's PSM.
  using ChannelDeletedCallback = fit::function<void()>;

  // Create a FakeDynamicChannel with Connection Handle |conn|, Protocol
  // Service Multiplexer (PSM) |psm| locally registered Channel ID |local_cid|,
  // and remote Channel ID |remote_cid|. Set to closed upon creation.
  FakeDynamicChannel(hci_spec::ConnectionHandle conn,
                     l2cap::Psm psm,
                     l2cap::ChannelId local_cid,
                     l2cap::ChannelId remote_cid);

  // Call the ChannelDeletedCallback instance associated with the server upon
  // destroying the channel instance.
  ~FakeDynamicChannel() {
    if (channel_deleted_callback_) {
      channel_deleted_callback_();
    }
  }

  void set_opened() { opened_ = true; }
  void set_closed() { opened_ = false; }
  void set_configuration_request_received() {
    configuration_request_received_ = true;
  }
  void set_configuration_response_received() {
    configuration_response_received_ = true;
  }
  void set_packet_handler_callback(
      PacketHandlerCallback packet_handler_callback) {
    packet_handler_callback_ = std::move(packet_handler_callback);
  }
  void set_send_packet_callback(SendPacketCallback send_packet_callback) {
    send_packet_callback_ = std::move(send_packet_callback);
  }
  void set_channel_deleted_callback(
      ChannelDeletedCallback channel_deleted_callback) {
    channel_deleted_callback_ = std::move(channel_deleted_callback);
  }

  hci_spec::ConnectionHandle handle() const { return handle_; }
  bool opened() const { return opened_; }
  bool configuration_request_received() const {
    return configuration_request_received_;
  }
  bool configuration_response_received() const {
    return configuration_response_received_;
  }
  l2cap::Psm psm() const { return psm_; }
  l2cap::ChannelId local_cid() const { return local_cid_; }
  l2cap::ChannelId remote_cid() const { return remote_cid_; }
  PacketHandlerCallback& packet_handler_callback() {
    return packet_handler_callback_;
  }
  SendPacketCallback& send_packet_callback() { return send_packet_callback_; }
  ChannelDeletedCallback& channel_deleted_callback() {
    return channel_deleted_callback_;
  }

  // Return a WeakPtr instance of this FakeDynamicChannel
  using WeakPtr = WeakSelf<FakeDynamicChannel>::WeakPtr;
  FakeDynamicChannel::WeakPtr AsWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  // ConnectionHandle associated with what
  hci_spec::ConnectionHandle handle_;

  // If the device is connected  and so is ready to communicate over the
  // channel.
  bool opened_;

  // If the initial ConfigurationRequest needed to open the channel has been
  // received. There must be a ConfigurationRequest and ConfigurationResponse
  // received in order to open up the channel.
  bool configuration_request_received_;

  // If the initial ConfigurationResponse needed to open the channel has been
  // received. There must be a ConfigurationRequest and ConfigurationResponse
  // received in order to open up the channel.
  bool configuration_response_received_;

  // The Protocol Service Multiplexer (PSM) associated with the channel.
  const l2cap::Psm psm_;

  // Identifies the local device's endpoint of this channel. Will be unique on
  // this device as long as this channel remains open.
  const l2cap::ChannelId local_cid_;

  // Identifies the endpoint of this channel on the peer device. Set upon
  // connection completion.
  const l2cap::ChannelId remote_cid_;

  // Callback associated with handling data packets sent to this channel.
  PacketHandlerCallback packet_handler_callback_;

  // Callback associated with sending data packets using this channel.
  SendPacketCallback send_packet_callback_;

  // Callback associated with closing the dynamic channel.
  ChannelDeletedCallback channel_deleted_callback_;

  // Any management of FakeDynamicChannel instances outside of FakeL2cap
  // should be done through the use of WeakPtrs.
  WeakSelf<FakeDynamicChannel> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeDynamicChannel);
};

}  // namespace bt::testing
