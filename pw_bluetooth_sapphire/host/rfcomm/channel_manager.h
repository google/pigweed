// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_MANAGER_H_

#include <tuple>
#include <unordered_map>

#include <fbl/ref_ptr.h>
#include <lib/fxl/memory/weak_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/rfcomm/session.h"

namespace bt {
namespace rfcomm {

// The main entry point for managing RFCOMM connections. ChannelManager has
// functions for connecting to remote RFCOMM channels and listening to
// connections on local channels.
//
// THREAD SAFETY:
//
// ChannelManager is not thread safe, and should only be accessed from the
// thread it was created on. ChannelManager dispatches its tasks to the default
// dispatcher of the thread it was created on.
class ChannelManager {
 public:
  // |l2cap_delegate| is responsible for initiating a connection-oriented L2CAP
  // channel for the RFCOMM PSM and reporting the resultant channel in a
  // ChannelCallback. This is invoked by OpenRemoteChannel to initiate a new
  // RFCOMM session when a session does not exist. The ChannelCallback parameter
  // of |l2cap_delegate| must run on this ChannelManager's creation thread.
  //
  // TODO(armansito): Consider separating the concern of initiating an L2CAP
  // connection from managing RFCOMM sessions by removing this dependency by
  // making data::Domain take care of the former.
  using OnL2capConnectionRequest =
      fit::function<void(hci::ConnectionHandle, l2cap::ChannelCallback)>;
  explicit ChannelManager(OnL2capConnectionRequest l2cap_delegate);

  // Registers |l2cap_channel| with RFCOMM. After this call, we will be able to
  // use OpenRemoteChannel() to get an RFCOMM channel multiplexed on top of this
  // L2CAP channel. Returns true on success, false otherwise.
  bool RegisterL2CAPChannel(fbl::RefPtr<l2cap::Channel> l2cap_channel);

  // This callback will be used to transfer ownership of a new RFCOMM Channel
  // object. It is used to deliver both incoming channels (initiated by the
  // remote) and outgoing channels. Failure will be indicated by a value of
  // kInvalidServerChannel as the ServerChannel, and a nullptr as the Channel
  // pointer.
  using ChannelOpenedCallback =
      fit::function<void(fbl::RefPtr<Channel>, ServerChannel)>;

  // Open an outgoing RFCOMM channel to the remote device represented by
  // |handle|. If a session corresponding to |handle| does not exist, a new
  // L2CAP connection to the RFCOMM PSM will be requested by invoking the
  // |l2cap_delegate| passed to the constructor.
  void OpenRemoteChannel(hci::ConnectionHandle handle,
                         ServerChannel server_channel,
                         ChannelOpenedCallback channel_opened_cb,
                         async_dispatcher_t* dispatcher);

  // Reserve an incoming RFCOMM channel, and get its Server Channel. Any
  // incoming RFCOMM channels opened with this Server Channel will be
  // given to |cb|. Returns the Server Channel allocated on success, and
  // kInvalidServerChannel otherwise.
  ServerChannel AllocateLocalChannel(ChannelOpenedCallback cb,
                                     async_dispatcher_t* dispatcher);

 private:
  // Calls the appropriate callback for |server_channel|, passing in
  // |rfcomm_channel|.
  void ChannelOpened(fbl::RefPtr<Channel> rfcomm_channel,
                     ServerChannel server_channel);

  // Holds callbacks for Server Channels allocated via AllocateLocalChannel.
  std::unordered_map<ServerChannel,
                     std::pair<ChannelOpenedCallback, async_dispatcher_t*>>
      server_channels_;

  // Maps open connections to open RFCOMM sessions.
  std::unordered_map<hci::ConnectionHandle, std::unique_ptr<Session>>
      handle_to_session_;

  // The dispatcher which ChannelManager uses to run its own tasks.
  async_dispatcher_t* const dispatcher_;
  const OnL2capConnectionRequest l2cap_delegate_;

  fxl::WeakPtrFactory<ChannelManager> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ChannelManager);
};

}  // namespace rfcomm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_RFCOMM_CHANNEL_MANAGER_H_
