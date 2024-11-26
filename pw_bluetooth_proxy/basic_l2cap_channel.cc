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
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

pw::Result<BasicL2capChannel> BasicL2capChannel::Create(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t local_cid,
    pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn) {
  if (!L2capReadChannel::AreValidParameters(connection_handle, local_cid)) {
    return pw::Status::InvalidArgument();
  }

  return BasicL2capChannel(/*l2cap_channel_manager=*/l2cap_channel_manager,
                           /*connection_handle=*/connection_handle,
                           /*local_cid=*/local_cid,
                           /*receive_fn=*/std::move(receive_fn));
}

BasicL2capChannel& BasicL2capChannel::operator=(BasicL2capChannel&& other) {
  L2capReadChannel::operator=(std::move(other));
  return *this;
}

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

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Received invalid B-frame. So will drop.",
                 local_cid());
  } else {
    CallReceiveFn(span(bframe_view->payload().BackingStorage().data(),
                       bframe_view->payload().SizeInBytes()));
  }

  return true;
}

}  // namespace pw::bluetooth::proxy
