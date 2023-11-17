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

#include "pw_bluetooth_sapphire/internal/host/testing/fake_sdp_server.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/testing/fake_l2cap.h"

#pragma clang diagnostic ignored "-Wshadow"

namespace bt::testing {

FakeSdpServer::FakeSdpServer(pw::async::Dispatcher& pw_dispatcher)
    : l2cap_(std::make_unique<l2cap::testing::FakeL2cap>(pw_dispatcher)),
      server_(l2cap_.get()) {}

void FakeSdpServer::RegisterWithL2cap(FakeL2cap* l2cap_) {
  auto channel_cb = [this](FakeDynamicChannel::WeakPtr channel) {
    auto handle_sdu = [this, channel](auto& request) {
      if (channel.is_alive()) {
        HandleSdu(channel, request);
      }
    };
    channel->set_packet_handler_callback(handle_sdu);
  };
  l2cap_->RegisterService(l2cap::kSDP, channel_cb);
}

void FakeSdpServer::HandleSdu(FakeDynamicChannel::WeakPtr channel,
                              const ByteBuffer& sdu) {
  BT_ASSERT(channel->opened());
  auto response = server()->HandleRequest(
      std::make_unique<DynamicByteBuffer>(sdu), l2cap::kDefaultMTU);
  if (response) {
    auto& callback = channel->send_packet_callback();
    return callback(*response.value());
  }
}

}  // namespace bt::testing
