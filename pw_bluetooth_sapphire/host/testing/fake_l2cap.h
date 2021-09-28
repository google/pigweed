// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_

#include <lib/fit/function.h>

#include <unordered_map>

#include "fake_dynamic_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::testing {

// This class unpacks data units received from ACL-U and LE-U logical links
// into L2CAP SDUs and then routes them to indvidually-registered handler
// functions. Each FakePeer should have its own FakeL2cap instance and its
// own set of ACL-U and LE-U logical links.
// This class also manages the dynamic L2CAP channels, stored in
// FakeDynamicChannel objects.
// Generally, dynamic channels are created by means of ConnectionRequest packets
// sent over the fixed signaling channel, but FakeL2cap actually holds the
// channels created and modified by those requests.
// Services can register with FakeL2cap and then assign callbacks to individual
// FakeDynamicChannels (based on their PSMs), but those services do not own and
// cannot directly delete or create new channels without using the FakeL2cap
// instance managing those dynamic channels.
class FakeL2cap final {
 public:
  // Entities that instantiate FakeL2cap must provide a
  // SendFrameCallback function to handle adding necessary protocol data unit
  // header information to the packet and actually sending the packet using
  // the associated device.
  using SendFrameCallback = fit::function<void(hci_spec::ConnectionHandle conn,
                                               l2cap::ChannelId cid, const ByteBuffer& pdu)>;

  // After registering a channel with RegisterHandler, ChannelReceiveCallback
  // is called with a received L2CAP Service Data Unit |sdu| when FakeL2cap
  // handles a packet for the registered channel. Includes the |handle| that
  // the L2CAP packet was received on.
  using ChannelReceiveCallback =
      fit::function<void(hci_spec::ConnectionHandle handle, const ByteBuffer& sdu)>;

  // When FakeL2cap receives a packet with a ChannelID that does not have a
  // registered handler, it calls UnexpectedPduCallback (set via the
  // constructor or defaulted to a no-op). To aid with debugging, this callback
  // takes the entire Protocol Data Unit |pdu| (including the intact L2CAP
  // header). Also includes the |handle| that the L2CAP packet was received on.
  using UnexpectedPduCallback =
      fit::function<void(hci_spec::ConnectionHandle handle, const ByteBuffer& pdu)>;

  // Each service that registers itself on a particular PSM provides a callback
  // that takes a pointer to the FakeDynamicChannel with its
  using FakeDynamicChannelCallback = fit::function<void(fxl::WeakPtr<FakeDynamicChannel>)>;

  // Calls |send_frame_callback| to send packets through the device
  // instantiating the FakeL2cap instance.
  // Calls |unexpected_pdu_callback| for packets received that don't have a
  // handler registered. Defaults to a no-op if no callback provided.
  // Assumes the largest possible dynamic channel ID is
  // l2cap::kLastACLDynamicChannelId.
  explicit FakeL2cap(
      SendFrameCallback send_frame_callback,
      UnexpectedPduCallback unexpected_pdu_callback = [](auto handle, auto& pdu) {},
      l2cap::ChannelId largest_channel_id = l2cap::kLastACLDynamicChannelId);

  // Public methods for services/clients that operate over L2CAP channels:
  // TODO (fxbug.dev/57535) Add Unit tests for public methods

  // Register |callback| to be called when a Service Data Unit (SDU) is
  // received on an L2CAP channel identified by |cid|. |callback| will be
  // retained until overwritten by another RegisterHandler call or this
  // class is destroyed. To remove a specific |callback|, overwrite it by
  // registering a no-op (or other handler) on the corresponding |cid|.
  // This should only be used for registering fixed channel handlers - use
  // RegisterDynamicChannel for dynamic channel management.
  void RegisterHandler(l2cap::ChannelId cid, ChannelReceiveCallback callback);

  // Register a |callback| function that configures a FakeDynamicChannel which
  // operates over a Protocol Service Multiplexer |psm|. When
  // RegisterDynamicChannelWithPsm() will call this FakeDynamicChannelCallback
  // if the channel is open.
  void RegisterService(l2cap::PSM psm, FakeDynamicChannelCallback callback);

  // The following methods are generally for use by the FakeSignalingServer when
  // creating and managing individual FakeDynamicChannel instances.

  // Register a new FakeDynamicChannel with an associated connection handle
  // |conn|, Protocol Service Multiplexer PSM, remote Channel ID
  // |remote_cid|, and local Channel ID |local_cid|.
  // This channel will be closed, as this function only allocates
  // a local ID for the channel. This function also does not register the
  // channel with the service associated with the channel's PSM, as this
  // happens after the channel is open.
  // Return status of if the registration was successful.
  // This should only be used for registering dynamic channels - use
  // RegisterHandler for fixed channel management.
  bool RegisterDynamicChannel(hci_spec::ConnectionHandle conn, l2cap::PSM psm,
                              l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid);

  // Call the DynamicChannelCallback associated with the service operating
  // on the channel's PSM once this channel has been opened.
  // Applications should only call this function after confirming that the
  // channel associated with the connection handle |conn| and local channel ID
  // |local_cid| is already open, as the DynamicChannelCallback associated with
  // the PSM will assume that it can operate over the channel.
  // Return status of if the registration was successful.
  bool RegisterDynamicChannelWithPsm(hci_spec::ConnectionHandle conn, l2cap::ChannelId local_cid);

  // Return true if there is a FakeDynamicChannelCallback registered for
  // Protocol Service Multiplexer |psm|, return false otherwise.
  bool ServiceRegisteredForPsm(l2cap::PSM psm);

  // Methods to observe and manipulate L2CAP channel states:

  // Return the first local dynamic Channel Id that does not already have a
  // FakeDynamicChannel object associated with it. This incrementally checks
  // each individual ID starting from l2cap::kFirstDynamicChannelId up to this
  // FakeL2cap instance's largest_channel_id_, so runtime increases linearly
  // with the number of consecutive registered channels but can be limited by
  // limiting the value of largest_channel_id_ when initializing FakeL2cap. If
  // there are no available Channel IDs, returns l2cap::kInvalidChannelId.
  // Requires identification of the specific ConnectionHandle |conn| associated
  // with this connection.
  l2cap::ChannelId FindAvailableDynamicChannelId(hci_spec::ConnectionHandle conn);

  // Find the FakeDynamicChannel object with the local channel ID |local_cid|
  // and connection handle |conn| in the dynamic_channels_ map. If there is no
  // channel registered with the |local_cid|, return a nullptr.
  fxl::WeakPtr<FakeDynamicChannel> FindDynamicChannelByLocalId(hci_spec::ConnectionHandle conn,
                                                               l2cap::ChannelId local_cid);

  // Find the FakeDynamicChannel object with the connection handle |conn| and
  // local channel ID |remote_cid| in the dynamic_channels_ map. If there is
  // no channel registered with the |remote_cid|, return a nullptr.
  fxl::WeakPtr<FakeDynamicChannel> FindDynamicChannelByRemoteId(hci_spec::ConnectionHandle conn,
                                                                l2cap::ChannelId remote_cid);

  // Remove a dynamic channel associated with the connection handle |conn| and locassigned
  // |local_cid|. Will call the channel's ChannelClosedCallback if it has one, set the channel to
  // closed, and then delete the value from the map FakeL2cap uses to store
  // the channels (which should also destroy the channel itself),
  // This is the only way that dynamic channels should be deleted - if they are
  // torn down individually, FakeL2cap will incorrectly hold that local_cid.
  void DeleteDynamicChannelByLocalId(hci_spec::ConnectionHandle conn, l2cap::ChannelId local_cid);

  // Routes the |pdu| to the appropriate calllback function by extracting the
  // ChannelID of the received packet |pdu| and calling the corresponding
  // registered handler function (and providing it with the |handle| the packet
  // was received on and the payload Service Data Unit |sdu|.
  void HandlePdu(hci_spec::ConnectionHandle conn, const ByteBuffer& pdu);

  // Return the SendFrameCallback associated with this FakeL2cap instance.
  SendFrameCallback& send_frame_callback() { return send_frame_callback_; }

 private:
  // Map of channel IDs and corresponding functions. Use an unordered map for
  // constant-time (in the best case) for search/insertion/deletion when
  // accessing calllbacks associated with specific channel IDs.
  std::unordered_map<l2cap::ChannelId, ChannelReceiveCallback> callbacks_;

  // Map of dynamically allocated Channel IDs and corresponding channels. Use
  // an unordered map for constant-time search/insertion/deletion when
  // accessing channels associated with specific channel IDs.
  std::unordered_map<hci_spec::ConnectionHandle,
                     std::unordered_map<l2cap::ChannelId, std::unique_ptr<FakeDynamicChannel>>>
      dynamic_channels_;

  // Map of individual channel configuration callbacks associated with
  // individual services
  std::unordered_map<l2cap::PSM, FakeDynamicChannelCallback> registered_services_;

  // Function provided by the device that instantiates the FakeL2cap instance
  // that adds PDU header information and actually sends the packet.
  SendFrameCallback send_frame_callback_;

  // Handler function associated with unexpected PDUs. Defaults to a no-op.
  UnexpectedPduCallback unexpected_pdu_callback_;

  // Largest dynamic channel ID this FakeL2cap instance can allocate IDs for.
  // Defaults to l2cap::kLastACLDynamicChannelId.
  l2cap::ChannelId largest_channel_id_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeL2cap);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_L2CAP_H_
