// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async-testing/test_loop.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_dynamic_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"

BT_DECLARE_FAKE_DRIVER();

constexpr static bt::hci::ConnectionHandle kTestHandle = 0x0001;

bt::l2cap::ChannelParameters ConsumeChannelParameters(FuzzedDataProvider& provider) {
  bt::l2cap::ChannelParameters params;

  bool use_defaults = provider.ConsumeBool();
  if (use_defaults) {
    return params;
  }

  params.mode = provider.ConsumeBool() ? bt::l2cap::ChannelMode::kBasic
                                       : bt::l2cap::ChannelMode::kEnhancedRetransmission;
  params.max_rx_sdu_size = provider.ConsumeIntegral<uint16_t>();
  return params;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  bt::UsePrintf(bt::LogSeverity::ERROR);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Sets dispatcher needed for signaling channel reponse timeout.
  async::TestLoop loop;

  auto fake_chan = fbl::AdoptRef(new bt::l2cap::testing::FakeChannel(
      bt::l2cap::kSignalingChannelId, bt::l2cap::kSignalingChannelId, kTestHandle,
      bt::hci::Connection::LinkType::kACL));

  bt::l2cap::internal::BrEdrSignalingChannel sig_chan(fake_chan,
                                                      bt::hci::Connection::Role::kMaster);

  auto open_cb = [](auto chan) {};
  auto close_cb = [](auto chan) {};
  auto service_chan_cb = [](auto chan) {};

  auto service_cb = [&](auto psm) {
    // Reject some PSMs.
    if (provider.ConsumeBool()) {
      return std::optional<bt::l2cap::internal::DynamicChannelRegistry::ServiceInfo>();
    }

    auto params = ConsumeChannelParameters(provider);
    return std::optional(
        bt::l2cap::internal::DynamicChannelRegistry::ServiceInfo(params, service_chan_cb));
  };
  bt::l2cap::internal::BrEdrDynamicChannelRegistry registry(&sig_chan, close_cb, service_cb,
                                                            /*random_channel_ids=*/true);

  while (provider.remaining_bytes() > 0) {
    // Receive an l2cap packet.
    uint16_t data_size = provider.ConsumeIntegral<uint16_t>();
    auto data = provider.ConsumeBytes<uint8_t>(data_size);
    fake_chan->Receive(bt::BufferView(data.data(), data.size()));

    if (provider.ConsumeBool()) {
      registry.OpenOutbound(bt::l2cap::kAVDTP, ConsumeChannelParameters(provider), open_cb);
    }

    if (provider.ConsumeBool()) {
      loop.RunFor(bt::l2cap::kSignalingChannelResponseTimeout);
    }
  }

  return 0;
}
