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

#include "pw_bluetooth_proxy/single_channel_proxy.h"

#include "pw_bluetooth_proxy/channel_proxy.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

SingleChannelProxy::SingleChannelProxy(
    L2capChannelManager& l2cap_channel_manager,
    multibuf::MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn)
    : L2capChannel(
          l2cap_channel_manager,
          rx_multibuf_allocator,
          /*connection_handle=*/connection_handle,
          /*transport=*/transport,
          /*local_cid=*/local_cid,
          /*remote_cid=*/remote_cid,
          /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
          /*payload_from_host_fn=*/std::move(payload_from_host_fn)),
      L2capChannel::Holder(/*underlying_channel=*/this),
      ChannelProxy(/*event_fn=*/std::move(event_fn)) {
  // TODO: https://pwbug.dev/388082771 - Adjust log parameters once we are done
  // with transition.
  PW_LOG_INFO("btproxy: SingleChannelProxy ctor - this: %p, this(Holder): %p",
              (void*)this,
              (void*)(L2capChannel::Holder*)this);

  // Verify L2capChannel::holder_ and Holder::underlying_channel_ were properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
}

SingleChannelProxy::SingleChannelProxy(SingleChannelProxy&& other)
    : L2capChannel(std::move(static_cast<L2capChannel&>(other))),
      L2capChannel::Holder(
          std::move(static_cast<L2capChannel::Holder&>(other))),
      ChannelProxy(std::move(static_cast<ChannelProxy&>(other))) {
  // TODO: https://pwbug.dev/388082771 - To delete. Just using during
  // transition.
  PW_LOG_INFO(
      "btproxy: SingleChannelProxy move ctor after - this: %p, this(Holder): "
      "%p",
      (void*)this,
      (void*)(L2capChannel::Holder*)this);
  // Verify L2capChannel::holder_ and Holder::underlying_channel_ were properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
}

SingleChannelProxy& SingleChannelProxy::operator=(SingleChannelProxy&& other) {
  if (this != &other) {
    PW_LOG_INFO(
        "btproxy: SingleChannelProxy move= other - this: %p, this(Holder): "
        "%p",
        (void*)&other,
        (void*)(L2capChannel::Holder*)&other);

    L2capChannel::operator=(std::move(static_cast<L2capChannel&>(other)));
    L2capChannel::Holder::operator=(
        std::move(static_cast<L2capChannel::Holder&>(other)));
    ChannelProxy::operator=(std::move(static_cast<ChannelProxy&>(other)));

    PW_LOG_INFO(
        "btproxy: SingleChannelProxy move= after - this: %p, this(Holder): "
        "%p",
        (void*)this,
        (void*)(L2capChannel::Holder*)this);
  }
  // Verify L2capChannel::holder_ and Holder::underlying_channel_ are properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
  return *this;
}

SingleChannelProxy::~SingleChannelProxy() {
  // Log dtor unless this is a moved from object.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: SingleChannelProxy dtor");
  }
}

void SingleChannelProxy::HandleUnderlyingChannelEvent(L2capChannelEvent event) {
  SendEventToClient(event);
}

}  // namespace pw::bluetooth::proxy
