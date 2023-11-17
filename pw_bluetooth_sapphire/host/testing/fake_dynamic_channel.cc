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

#include "pw_bluetooth_sapphire/internal/host/testing/fake_dynamic_channel.h"

namespace bt::testing {

FakeDynamicChannel::FakeDynamicChannel(hci_spec::ConnectionHandle conn,
                                       l2cap::Psm psm,
                                       l2cap::ChannelId local_cid,
                                       l2cap::ChannelId remote_cid)
    : handle_(conn),
      opened_(false),
      configuration_request_received_(false),
      configuration_response_received_(false),
      psm_(psm),
      local_cid_(local_cid),
      remote_cid_(remote_cid),
      weak_self_(this) {}

}  // namespace bt::testing
