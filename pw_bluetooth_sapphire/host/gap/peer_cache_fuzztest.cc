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
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  pw::async::test::FakeDispatcher dispatcher;

  FuzzedDataProvider fuzzed_data_provider(data, size);
  pw::random::FuzzerRandomGenerator rng(&fuzzed_data_provider);
  bt::set_random_generator(&rng);

  bt::gap::PeerCache peer_cache(dispatcher);
  bt::gap::Peer *const peer =
      peer_cache.NewPeer(bt::testing::MakePublicDeviceAddress(fuzzed_data_provider),
                         fuzzed_data_provider.ConsumeBool());
  bt::gap::testing::PeerFuzzer peer_fuzzer(fuzzed_data_provider, *peer);
  while (fuzzed_data_provider.remaining_bytes() != 0) {
    peer_fuzzer.FuzzOneField();
    if (fuzzed_data_provider.ConsumeBool()) {
      dispatcher.RunUntilIdle();
    }
  }
  return 0;
}
