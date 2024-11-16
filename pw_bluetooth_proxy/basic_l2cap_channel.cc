// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_proxy/basic_l2cap_channel.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"

namespace pw::bluetooth::proxy {

BasicL2capChannel::BasicL2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t local_cid,
    pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn)
    : L2capReadChannel(l2cap_channel_manager,
                       std::move(receive_fn),
                       connection_handle,
                       local_cid) {}

bool BasicL2capChannel::OnPduReceived(pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);
  CallReceiveFn(span(bframe_view->payload().BackingStorage().data(),
                     bframe_view->BackingStorage().SizeInBytes()));
  return true;
}

}  // namespace pw::bluetooth::proxy
