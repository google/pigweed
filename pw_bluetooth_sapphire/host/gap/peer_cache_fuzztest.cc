// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <pw_async/fake_dispatcher.h>
#include <pw_random/fuzzer.h>

#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/peer_fuzzer.h"

// Lightweight harness that adds a single peer to a PeerCache and mutates it with fuzz inputs
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  pw::random::FuzzerRandomGenerator rng(&fuzzed_data_provider);
  bt::set_random_generator(&rng);

  pw::async::test::FakeDispatcher dispatcher;
  bt::gap::PeerCache peer_cache(dispatcher);

  bt::DeviceAddress addr = bt::testing::MakePublicDeviceAddress(fuzzed_data_provider);
  bool connectable = fuzzed_data_provider.ConsumeBool();
  // NewPeer() can get stuck in an infinite loop generating a PeerId if there is
  // no fuzzer data left.
  if (fuzzed_data_provider.remaining_bytes() == 0) {
    bt::set_random_generator(nullptr);
    return 0;
  }
  bt::gap::Peer* const peer = peer_cache.NewPeer(addr, connectable);

  bt::gap::testing::PeerFuzzer peer_fuzzer(fuzzed_data_provider, *peer);
  while (fuzzed_data_provider.remaining_bytes() != 0) {
    peer_fuzzer.FuzzOneField();
    if (fuzzed_data_provider.ConsumeBool()) {
      dispatcher.RunUntilIdle();
    }
  }
  bt::set_random_generator(nullptr);
  return 0;
}
