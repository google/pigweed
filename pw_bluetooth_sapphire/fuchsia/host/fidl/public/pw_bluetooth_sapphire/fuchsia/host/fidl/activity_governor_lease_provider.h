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

#pragma once

#include <fidl/fuchsia.power.system/cpp/fidl.h>

#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/common/inspectable.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/lease.h"

namespace bthost {

// This class is not thread safe.
// Leases vended by ActivityGovernorLeaseProvider will only be valid as long as
// ActivityGovernorLeaseProvider is alive.
class ActivityGovernorLeaseProvider final
    : public pw::bluetooth_sapphire::LeaseProvider,
      public fidl::Server<::fuchsia_power_system::SuspendBlocker> {
 public:
  static std::unique_ptr<ActivityGovernorLeaseProvider> Create(
      fidl::ClientEnd<::fuchsia_power_system::ActivityGovernor>
          activity_governor_client,
      async_dispatcher_t* dispatcher);

  ActivityGovernorLeaseProvider(
      fidl::ClientEnd<::fuchsia_power_system::ActivityGovernor>
          activity_governor_client,
      fidl::ServerEnd<::fuchsia_power_system::SuspendBlocker>
          suspend_blocker_server,
      async_dispatcher_t* dispatcher);
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ActivityGovernorLeaseProvider);

  ~ActivityGovernorLeaseProvider() override = default;
  pw::Result<pw::bluetooth_sapphire::Lease> Acquire(const char* name) override;

  void AttachInspect(inspect::Node& parent, const char* name);

 private:
  enum class State {
    kResumed,
    kSuspending,
  };

  void OnLeaseDropped();
  void AcquireWakeLease();

  // fidl::Server<::fuchsia_power_system::SuspendBlocker> overrides:
  void BeforeSuspend(BeforeSuspendCompleter::Sync& completer) override;
  void AfterResume(AfterResumeCompleter::Sync& completer) override;
  void handle_unknown_method(
      fidl::UnknownMethodMetadata<::fuchsia_power_system::SuspendBlocker>
          metadata,
      fidl::UnknownMethodCompleter::Sync& completer) override {}

  inspect::Node node_;
  State state_ = State::kResumed;
  fidl::SyncClient<fuchsia_power_system::ActivityGovernor> governor_;
  bt::BoolInspectable<std::optional<::fuchsia_power_system::LeaseToken>> token_;
  uint16_t ref_count_ = 0;
  std::optional<fidl::ServerBindingRef<::fuchsia_power_system::SuspendBlocker>>
      binding_ref_;
  WeakSelf<ActivityGovernorLeaseProvider> weak_ptr_factory_{this};
};

}  // namespace bthost
