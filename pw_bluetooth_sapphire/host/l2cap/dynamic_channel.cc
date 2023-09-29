// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dynamic_channel.h"

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_dynamic_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

DynamicChannel::DynamicChannel(DynamicChannelRegistry* registry, Psm psm, ChannelId local_cid,
                               ChannelId remote_cid)
    : registry_(registry),
      psm_(psm),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      opened_(false) {
  BT_DEBUG_ASSERT(registry_);
}

bool DynamicChannel::SetRemoteChannelId(ChannelId remote_cid) {
  // do not allow duplicate remote CIDs
  auto channel = registry_->FindChannelByRemoteId(remote_cid);
  if (channel && channel != this) {
    bt_log(WARN, "l2cap",
           "channel %#.4x: received remote channel id %#.4x that is already set for channel %#.4x",
           local_cid(), remote_cid, channel->local_cid());
    return false;
  }

  remote_cid_ = remote_cid;
  return true;
}

void DynamicChannel::OnDisconnected() { registry_->OnChannelDisconnected(this); }

}  // namespace bt::l2cap::internal
