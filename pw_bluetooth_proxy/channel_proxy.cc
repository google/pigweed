// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_proxy/channel_proxy.h"

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

void ChannelProxy::SendEventToClient(L2capChannelEvent event) {
  // We don't log kWriteAvailable since they happen often. Optimally we would
  // just debug log them also, but one of our downstreams logs all levels.
  if (event != L2capChannelEvent::kWriteAvailable) {
    // TODO: https://pwbug.dev/388082771 - Add channel identifying information
    // here once/if ChannelProxy has access to it (e.g. via L2capChannel ref).
    PW_LOG_INFO("btproxy: ChannelProxy::SendEventToClient - event: %u",
                cpp23::to_underlying(event));
  }

  if (event_fn_) {
    event_fn_(event);
  }
}

}  // namespace pw::bluetooth::proxy
