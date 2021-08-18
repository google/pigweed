// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/host_server.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/peer_fuzzer.h"

namespace bthost {
namespace {

class HostServerFuzzTest final : public bthost::testing::AdapterTestFixture {
 public:
  HostServerFuzzTest() { SetUp(); }

  ~HostServerFuzzTest() override {
    host_ = nullptr;
    host_server_ = nullptr;
    gatt_ = nullptr;
    AdapterTestFixture::TearDown();
  }

  // Creates a peer with fuzzer-generated mutations that will be encoded as a
  // fuchsia.bluetooth.sys.Peer and sent as a response to WatchPeers immediately (see
  // HostServerTest.WatchPeersRepliesOnFirstCallWithExistingPeers).
  void FuzzWatchPeers(FuzzedDataProvider &fuzzed_data_provider) {
    // WatchPeers only tracks connectable peers by design
    bt::gap::Peer *const peer =
        adapter()->peer_cache()->NewPeer(bt::testing::MakePublicDeviceAddress(fuzzed_data_provider),
                                         /*connectable=*/true);
    ZX_ASSERT(peer);
    bt::gap::testing::PeerFuzzer peer_fuzzer(fuzzed_data_provider, *peer);
    while (fuzzed_data_provider.remaining_bytes() != 0) {
      peer_fuzzer.FuzzOneField();
    }

    // TODO(fxbug.dev/65461): WatchPeers will trigger this test as a failure if we try to encode a
    // lot of peers, even though fuzzing multiple peers would be helpful.
    int watch_peers_responses = 0;
    host_client()->WatchPeers([this, peer, &watch_peers_responses](auto updated, auto removed) {
      ZX_ASSERT_MSG(updated.size() == 1, "peer %s: peers updated = %zu", bt_str(*peer),
                    updated.size());
      ZX_ASSERT_MSG(removed.size() == 0, "peer %s: peers removed = %zu", bt_str(*peer),
                    removed.size());
      HandleWatchPeersResponse(*host_client(), watch_peers_responses, /*max_call_depth=*/1,
                               std::move(updated), std::move(removed));
    });
    RunLoopUntilIdle();
    ZX_ASSERT_MSG(watch_peers_responses == 1, "peer %s: WatchPeers returned %d times",
                  bt_str(*peer), watch_peers_responses);
  }

 private:
  HostServer *host_server() const { return host_server_.get(); }

  fuchsia::bluetooth::host::Host *host_client() const { return host_.get(); }

  void SetUp() override {
    AdapterTestFixture::SetUp();
    gatt_ = take_gatt();
    fidl::InterfaceHandle<fuchsia::bluetooth::host::Host> host_handle;
    host_server_ = std::make_unique<HostServer>(host_handle.NewRequest().TakeChannel(),
                                                adapter()->AsWeakPtr(), gatt_->AsWeakPtr());
    host_.Bind(std::move(host_handle));
  }

  // WatchPeers response handler that can re-initiate the call per the "hanging get" pattern up to
  // |max_call_depth| times, like a normal client might.
  template <typename Host>
  void HandleWatchPeersResponse(Host &host, int &call_counter, int max_call_depth,
                                std::vector<fuchsia::bluetooth::sys::Peer> updated,
                                std::vector<fuchsia::bluetooth::PeerId> removed) {
    call_counter++;
    ZX_ASSERT_MSG(call_counter <= max_call_depth, "max depth (%d) exceeded", call_counter);
    host.WatchPeers([this, &host, &call_counter, max_call_depth](auto updated, auto removed) {
      this->HandleWatchPeersResponse(host, call_counter, max_call_depth, std::move(updated),
                                     std::move(removed));
    });
  }

  // No-op impl for the GoogleTest base class
  void TestBody() override {}

  std::unique_ptr<bt::gatt::GATT> gatt_;
  std::unique_ptr<HostServer> host_server_;
  fuchsia::bluetooth::host::HostPtr host_;
};

}  // namespace
}  // namespace bthost

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  bthost::HostServerFuzzTest host_server_fuzz_test;
  host_server_fuzz_test.FuzzWatchPeers(fuzzed_data_provider);
  return 0;
}
