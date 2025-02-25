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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/activity_governor_lease_provider.h"

#include "gmock/gmock.h"
#include "lib/async-loop/cpp/loop.h"
#include "lib/async/cpp/task.h"

namespace bthost {
namespace {

class FakeActivityGovernor final
    : public fidl::Server<fuchsia_power_system::ActivityGovernor> {
 public:
  FakeActivityGovernor(
      fidl::ServerEnd<fuchsia_power_system::ActivityGovernor> server_end,
      async::Loop& loop)
      : loop_(loop) {
    binding_ref_.emplace(
        fidl::BindServer(loop.dispatcher(), std::move(server_end), this));
  }

  void AcquireWakeLease(AcquireWakeLeaseRequest& request,
                        AcquireWakeLeaseCompleter::Sync& completer) override {
    // Acquiring multiple leases should only result in 1 AcquireWakeLease call.
    EXPECT_EQ(lease_count_, 0u);
    if (lease_count_ != 0) {
      loop_.Quit();
      return;
    }
    EXPECT_EQ(request.name(), "bt-host");
    lease_count_++;
    zx::eventpair endpoint0, endpoint1;
    zx::eventpair::create(/*options=*/0, &endpoint0, &endpoint1);
    completer.Reply(fit::ok(std::move(endpoint0)));
    endpoint1.wait_one(
        ZX_EVENTPAIR_PEER_CLOSED, zx::time::infinite(), /*pending=*/nullptr);
    loop_.Quit();
  }
  void handle_unknown_method(
      fidl::UnknownMethodMetadata<fuchsia_power_system::ActivityGovernor>
          metadata,
      fidl::UnknownMethodCompleter::Sync& completer) override {}

 private:
  async::Loop& loop_;
  std::optional<fidl::ServerBindingRef<fuchsia_power_system::ActivityGovernor>>
      binding_ref_;
  uint8_t lease_count_ = 0;
};

void ServerTask(
    fidl::ServerEnd<fuchsia_power_system::ActivityGovernor> server_end) {
  async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);

  FakeActivityGovernor governor(std::move(server_end), loop);

  loop.Run();
}

TEST(ActivityGovernorLeaseProviderTest, Acquire) {
  auto endpoints =
      fidl::CreateEndpoints<fuchsia_power_system::ActivityGovernor>();
  ASSERT_TRUE(endpoints.is_ok());
  auto [client_end, server_end] = std::move(endpoints.value());
  ActivityGovernorLeaseProvider provider(std::move(client_end));

  std::thread server_thread(ServerTask, std::move(server_end));

  pw::Result<pw::bluetooth_sapphire::Lease> lease_0 =
      provider.Acquire("lease0");
  ASSERT_TRUE(lease_0.ok());
  pw::Result<pw::bluetooth_sapphire::Lease> lease_1 =
      provider.Acquire("lease1");
  ASSERT_TRUE(lease_1.ok());

  // destroy leases, signalling server thread to quit
  lease_0 = pw::Status::Unavailable();
  lease_1 = pw::Status::Unavailable();

  server_thread.join();
}

}  // namespace
}  // namespace bthost
