// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dynamic_channel.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"

namespace btlib {
namespace l2cap {
namespace internal {

DynamicChannel::DynamicChannel(DynamicChannelRegistry* registry, PSM psm,
                               ChannelId local_cid, ChannelId remote_cid)
    : registry_(registry),
      psm_(psm),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      opened_(false) {
  ZX_DEBUG_ASSERT(registry_);
}

void DynamicChannel::OnDisconnected() {
  registry_->OnChannelDisconnected(this);
}

}  // namespace internal
}  // namespace l2cap
}  // namespace btlib
