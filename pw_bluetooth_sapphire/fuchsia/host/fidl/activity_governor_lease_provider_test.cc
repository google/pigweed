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

#include <fidl/fuchsia.power.system/cpp/test_base.h>

#include "gmock/gmock.h"
#include "lib/async-loop/cpp/loop.h"
#include "lib/async/cpp/task.h"
#include "lib/async/cpp/wait.h"
#include "lib/async_patterns/testing/cpp/dispatcher_bound.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"

using SuspendBlocker = fuchsia_power_system::SuspendBlocker;

namespace bthost {
namespace {

class FakeActivityGovernor final
    : public fidl::testing::TestBase<fuchsia_power_system::ActivityGovernor> {
 public:
  FakeActivityGovernor(
      fidl::ServerEnd<fuchsia_power_system::ActivityGovernor> server_end,
      async_dispatcher_t* dispatcher) {
    binding_ref_.emplace(
        fidl::BindServer(dispatcher, std::move(server_end), this));
  }

  std::optional<fidl::ClientEnd<fuchsia_power_system::SuspendBlocker>>
  TakeSuspendBlocker() {
    std::optional<fidl::ClientEnd<fuchsia_power_system::SuspendBlocker>>
        blocker;
    blocker.swap(suspend_blocker_);
    return blocker;
  }

  uint8_t lease_count() const { return lease_count_; }

  std::optional<zx::eventpair> TakeLease() {
    std::optional<zx::eventpair> out;
    out.swap(wake_lease_);
    return out;
  }

 private:
  void AcquireWakeLease(AcquireWakeLeaseRequest& request,
                        AcquireWakeLeaseCompleter::Sync& completer) override {
    EXPECT_EQ(request.name(), "bt-host");
    lease_count_++;

    zx::eventpair endpoint0, endpoint1;
    zx::eventpair::create(/*options=*/0, &endpoint0, &endpoint1);
    wake_lease_ = std::move(endpoint1);

    completer.Reply(fit::ok(std::move(endpoint0)));
    ASSERT_TRUE(completer.result_of_reply().ok());
  }

  void RegisterSuspendBlocker(
      RegisterSuspendBlockerRequest& request,
      RegisterSuspendBlockerCompleter::Sync& completer) override {
    EXPECT_EQ(request.name(), "bt-host");
    ASSERT_TRUE(request.suspend_blocker().has_value());
    suspend_blocker_ = std::move(request.suspend_blocker().value());

    zx::eventpair endpoint0, endpoint1;
    zx::eventpair::create(/*options=*/0, &endpoint0, &endpoint1);
    completer.Reply(fit::ok(std::move(endpoint0)));
    ASSERT_TRUE(completer.result_of_reply().ok());
  }

  void handle_unknown_method(
      fidl::UnknownMethodMetadata<fuchsia_power_system::ActivityGovernor>
          metadata,
      fidl::UnknownMethodCompleter::Sync& completer) override {
    FAIL();
  }

  void NotImplemented_(const std::string& name,
                       ::fidl::CompleterBase& completer) override {
    FAIL();
  }

  std::optional<fidl::ServerBindingRef<fuchsia_power_system::ActivityGovernor>>
      binding_ref_;
  std::optional<fidl::ClientEnd<fuchsia_power_system::SuspendBlocker>>
      suspend_blocker_;
  uint8_t lease_count_ = 0;
  std::optional<zx::eventpair> wake_lease_;
};

class ActivityGovernorLeaseProviderTest : public bt::testing::TestLoopFixture {
 public:
  void SetUp() override {
    ASSERT_EQ(background_loop_.StartThread(), ZX_OK);

    auto endpoints =
        fidl::CreateEndpoints<fuchsia_power_system::ActivityGovernor>();
    ASSERT_TRUE(endpoints.is_ok());
    auto [client_end, server_end] = std::move(endpoints.value());
    fake_activity_governor_.emplace(std::move(server_end),
                                    async_patterns::PassDispatcher);

    provider_ = ActivityGovernorLeaseProvider::Create(std::move(client_end),
                                                      dispatcher());
    ASSERT_TRUE(provider_);

    std::optional<fidl::ClientEnd<fuchsia_power_system::SuspendBlocker>>
        suspend_blocker =
            governor().SyncCall(&FakeActivityGovernor::TakeSuspendBlocker);
    ASSERT_TRUE(suspend_blocker);
    suspend_blocker_.emplace(std::move(suspend_blocker.value()), dispatcher());
  }

  void TearDown() override { background_loop_.Shutdown(); }

  async_patterns::TestDispatcherBound<FakeActivityGovernor>& governor() {
    return fake_activity_governor_;
  }

  ActivityGovernorLeaseProvider& provider() { return *provider_; }

  fidl::Client<fuchsia_power_system::SuspendBlocker>& suspend_blocker() {
    return suspend_blocker_.value();
  }

 private:
  std::unique_ptr<ActivityGovernorLeaseProvider> provider_;
  std::optional<fidl::Client<fuchsia_power_system::SuspendBlocker>>
      suspend_blocker_;

  async::Loop background_loop_{&kAsyncLoopConfigNeverAttachToThread};
  async_patterns::TestDispatcherBound<FakeActivityGovernor>
      fake_activity_governor_{background_loop_.dispatcher()};
};

TEST_F(ActivityGovernorLeaseProviderTest,
       AcquireLeasesThenSuspendThenResumeThenReleaseLeases) {
  pw::Result<pw::bluetooth_sapphire::Lease> lease_0 =
      provider().Acquire("lease0");
  ASSERT_TRUE(lease_0.ok());
  pw::Result<pw::bluetooth_sapphire::Lease> lease_1 =
      provider().Acquire("lease1");
  ASSERT_TRUE(lease_1.ok());

  // No leases should be acquired before suspension.
  uint8_t lease_count = governor().SyncCall(&FakeActivityGovernor::lease_count);
  EXPECT_EQ(lease_count, 0u);

  // A real lease should be acquired during BeforeSuspend.
  int suspend_cb_count = 0;
  suspend_blocker()->BeforeSuspend().Then(
      [&](fidl::Result<SuspendBlocker::BeforeSuspend> result) {
        EXPECT_TRUE(result.is_ok());
        suspend_cb_count++;
      });
  RunLoopUntilIdle();
  EXPECT_EQ(suspend_cb_count, 1);
  lease_count = governor().SyncCall(&FakeActivityGovernor::lease_count);
  EXPECT_EQ(lease_count, 1u);

  std::optional<zx::eventpair> lease =
      governor().SyncCall(&FakeActivityGovernor::TakeLease);
  ASSERT_TRUE(lease);
  async::WaitOnce wait(lease->get(), ZX_EVENTPAIR_PEER_CLOSED);
  int lease_closed_count = 0;
  wait.Begin(dispatcher(),
             [&](auto, auto, auto, auto) { lease_closed_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(lease_closed_count, 0);

  // Resuming should not affect leases.
  int resume_cb_count = 0;
  suspend_blocker()->AfterResume().Then(
      [&](fidl::Result<SuspendBlocker::AfterResume> result) {
        EXPECT_TRUE(result.is_ok());
        resume_cb_count++;
      });
  RunLoopUntilIdle();
  EXPECT_EQ(resume_cb_count, 1);
  EXPECT_EQ(lease_closed_count, 0);

  // Destroying both leases should signal ActivityGovernor's lease eventpair.
  lease_0 = pw::Status::Cancelled();
  RunLoopUntilIdle();
  EXPECT_EQ(lease_closed_count, 0);
  lease_1 = pw::Status::Cancelled();
  RunLoopUntilIdle();
  EXPECT_EQ(lease_closed_count, 1);
}

TEST_F(ActivityGovernorLeaseProviderTest, AcquireLeaseAfterSuspend) {
  int suspend_cb_count = 0;
  suspend_blocker()->BeforeSuspend().Then(
      [&](fidl::Result<SuspendBlocker::BeforeSuspend> result) {
        EXPECT_TRUE(result.is_ok());
        suspend_cb_count++;
      });
  RunLoopUntilIdle();
  EXPECT_EQ(suspend_cb_count, 1);

  uint8_t lease_count = governor().SyncCall(&FakeActivityGovernor::lease_count);
  EXPECT_EQ(lease_count, 0u);

  // Lease acquired during suspension should acquire real lease.
  pw::Result<pw::bluetooth_sapphire::Lease> lease = provider().Acquire("lease");
  ASSERT_TRUE(lease.ok());
  lease_count = governor().SyncCall(&FakeActivityGovernor::lease_count);
  EXPECT_EQ(lease_count, 1u);

  std::optional<zx::eventpair> governor_lease =
      governor().SyncCall(&FakeActivityGovernor::TakeLease);
  ASSERT_TRUE(governor_lease);
  async::WaitOnce wait(governor_lease->get(), ZX_EVENTPAIR_PEER_CLOSED);
  int lease_closed_count = 0;
  wait.Begin(dispatcher(),
             [&](auto, auto, auto, auto) { lease_closed_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(lease_closed_count, 0);

  // Destroying lease should signal ActivityGovernor's lease eventpair.
  lease = pw::Status::Cancelled();
  RunLoopUntilIdle();
  EXPECT_EQ(lease_closed_count, 1);
}

}  // namespace
}  // namespace bthost
