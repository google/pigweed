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

#include "pw_bluetooth_sapphire/internal/host/l2cap/dynamic_channel.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/bredr_dynamic_channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/dynamic_channel_registry.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

DynamicChannel::DynamicChannel(DynamicChannelRegistry* registry,
                               Psm psm,
                               ChannelId local_cid,
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
    bt_log(WARN,
           "l2cap",
           "channel %#.4x: received remote channel id %#.4x that is already "
           "set for channel %#.4x",
           local_cid(),
           remote_cid,
           channel->local_cid());
    return false;
  }

  remote_cid_ = remote_cid;
  return true;
}

void DynamicChannel::OnDisconnected() {
  registry_->OnChannelDisconnected(this);
}

}  // namespace bt::l2cap::internal
