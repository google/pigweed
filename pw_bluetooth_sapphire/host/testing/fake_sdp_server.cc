// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
