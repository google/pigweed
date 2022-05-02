// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_sdp_server.h"

#include <endian.h>

#include "fake_l2cap.h"

namespace bt::testing {

FakeSdpServer::FakeSdpServer()
    : l2cap_(std::make_unique<l2cap::testing::FakeL2cap>()), server_(l2cap_.get()) {}

void FakeSdpServer::RegisterWithL2cap(FakeL2cap* l2cap_) {
  auto channel_cb = [this](fxl::WeakPtr<FakeDynamicChannel> channel) {
    auto handle_sdu = [this, channel](auto& request) {
      if (channel) {
        HandleSdu(channel, request);
      }
    };
    channel->set_packet_handler_callback(handle_sdu);
  };
  l2cap_->RegisterService(l2cap::kSDP, channel_cb);
  return;
}

void FakeSdpServer::HandleSdu(fxl::WeakPtr<FakeDynamicChannel> channel, const ByteBuffer& sdu) {
  ZX_ASSERT(channel->opened());
  auto response =
      server()->HandleRequest(std::make_unique<DynamicByteBuffer>(sdu), l2cap::kDefaultMTU);
  if (response) {
    auto& callback = channel->send_packet_callback();
    return callback(std::move(*response.value()));
  }
}

}  // namespace bt::testing
