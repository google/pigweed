// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dynamic_channel.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_dynamic_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace l2cap {
namespace internal {

DynamicChannel::DynamicChannel(DynamicChannelRegistry* registry, PSM psm, ChannelId local_cid,
                               ChannelId remote_cid)
    : registry_(registry),
      psm_(psm),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      opened_(false) {
  ZX_DEBUG_ASSERT(registry_);
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

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
