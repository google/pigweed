// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_dynamic_channel.h"

namespace bt {
namespace testing {

FakeDynamicChannel::FakeDynamicChannel(l2cap::PSM psm, l2cap::ChannelId local_cid,
                                       l2cap::ChannelId remote_cid)
    : opened_(false),
      configuration_request_received_(false),
      configuration_response_received_(false),
      psm_(psm),
      local_cid_(local_cid),
      remote_cid_(remote_cid) {}

}  // namespace testing
}  // namespace bt
