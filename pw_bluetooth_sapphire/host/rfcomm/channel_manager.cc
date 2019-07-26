// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel_manager.h"

#include <fbl/function.h>
#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

#include "session.h"

namespace bt {
namespace rfcomm {

ChannelManager::ChannelManager(OnL2capConnectionRequest l2cap_delegate)
    : dispatcher_(async_get_default_dispatcher()),
      l2cap_delegate_(std::move(l2cap_delegate)),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(l2cap_delegate_);
}

bool ChannelManager::RegisterL2CAPChannel(fbl::RefPtr<l2cap::Channel> l2cap_channel) {
  auto handle = l2cap_channel->link_handle();

  if (handle_to_session_.find(handle) != handle_to_session_.end()) {
    bt_log(WARN, "rfcomm", "L2CAP channel for link (handle: %#.4x) already registered", handle);
    return false;
  }

  auto session =
      Session::Create(l2cap_channel, fit::bind_member(this, &ChannelManager::ChannelOpened));
  if (!session) {
    bt_log(ERROR, "rfcomm", "could not start session on the given L2CAP channel");
    return false;
  }
  handle_to_session_[handle] = std::move(session);
  return true;
}

void ChannelManager::OpenRemoteChannel(hci::ConnectionHandle handle, ServerChannel server_channel,
                                       ChannelOpenedCallback channel_opened_cb,
                                       async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(channel_opened_cb);
  ZX_DEBUG_ASSERT(dispatcher);

  auto session_it = handle_to_session_.find(handle);
  if (session_it == handle_to_session_.end()) {
    l2cap_delegate_(handle, [this, handle, server_channel, dispatcher,
                             cb = std::move(channel_opened_cb)](auto l2cap_channel) mutable {
      if (!l2cap_channel) {
        bt_log(ERROR, "rfcomm", "failed to open L2CAP channel with handle %#.4x", handle);
        async::PostTask(dispatcher, [cb_ = std::move(cb)] { cb_(nullptr, kInvalidServerChannel); });
        return;
      }

      bt_log(INFO, "rfcomm", "opened L2CAP session with handle %#.4x", handle);
      ZX_DEBUG_ASSERT(handle_to_session_.find(handle) == handle_to_session_.end());

      handle_to_session_.emplace(
          handle,
          Session::Create(l2cap_channel, fbl::BindMember(this, &ChannelManager::ChannelOpened)));

      // Re-run OpenRemoteChannel now that the session is opened.
      async::PostTask(dispatcher_,
                      [this, handle, server_channel, dispatcher, cb_ = std::move(cb)]() mutable {
                        OpenRemoteChannel(handle, server_channel, std::move(cb_), dispatcher);
                      });
    });
    return;
  }

  ZX_DEBUG_ASSERT(session_it != handle_to_session_.end());

  session_it->second->OpenRemoteChannel(
      server_channel, [cb = std::move(channel_opened_cb), dispatcher](auto rfcomm_channel,
                                                                      auto server_channel) mutable {
        async::PostTask(dispatcher, [cb_ = std::move(cb), rfcomm_channel, server_channel]() {
          cb_(rfcomm_channel, server_channel);
        });
      });
}

ServerChannel ChannelManager::AllocateLocalChannel(ChannelOpenedCallback cb,
                                                   async_dispatcher_t* dispatcher) {
  // Find the first free Server Channel and allocate it.
  for (ServerChannel server_channel = kMinServerChannel; server_channel <= kMaxServerChannel;
       ++server_channel) {
    if (server_channels_.find(server_channel) == server_channels_.end()) {
      server_channels_[server_channel] = std::make_pair(std::move(cb), dispatcher);
      return server_channel;
    }
  }

  return kInvalidServerChannel;
}

void ChannelManager::ChannelOpened(fbl::RefPtr<Channel> rfcomm_channel,
                                   ServerChannel server_channel) {
  auto server_channel_it = server_channels_.find(server_channel);
  ZX_DEBUG_ASSERT_MSG(server_channel_it != server_channels_.end(),
                      "new channel created on an unallocated Server Channel");

  async::PostTask(server_channel_it->second.second,
                  [server_channel, rfcomm_channel, cb = server_channel_it->second.first.share()]() {
                    cb(rfcomm_channel, server_channel);
                  });
}

}  // namespace rfcomm
}  // namespace bt
