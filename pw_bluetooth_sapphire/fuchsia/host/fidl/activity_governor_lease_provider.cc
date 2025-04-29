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

#include "pw_assert/check.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

using SuspendBlocker = ::fuchsia_power_system::SuspendBlocker;
using ActivityGovernor = ::fuchsia_power_system::ActivityGovernor;

namespace bthost {

namespace {
constexpr const char* kLeaseName = "bt-host";
}

std::unique_ptr<ActivityGovernorLeaseProvider>
ActivityGovernorLeaseProvider::Create(
    fidl::ClientEnd<::fuchsia_power_system::ActivityGovernor>
        activity_governor_client,
    async_dispatcher_t* dispatcher) {
  fidl::SyncClient<ActivityGovernor> governor_client(
      std::move(activity_governor_client));

  auto suspend_blocker_endpoints = fidl::CreateEndpoints<SuspendBlocker>();
  if (suspend_blocker_endpoints.is_error()) {
    bt_log(ERROR, "fidl", "Failed to create SuspendBlocker endpoints");
    return nullptr;
  }

  fidl::Request<ActivityGovernor::RegisterSuspendBlocker> request;
  request.name() = kLeaseName;
  request.suspend_blocker() =
      std::move(suspend_blocker_endpoints.value().client);

  fidl::Result<ActivityGovernor::RegisterSuspendBlocker> result =
      governor_client->RegisterSuspendBlocker(std::move(request));
  if (result.is_error()) {
    bt_log(ERROR,
           "fidl",
           "Failed to register SuspendBlocker: %s",
           result.error_value().FormatDescription().c_str());
    return nullptr;
  }

  return std::make_unique<ActivityGovernorLeaseProvider>(
      governor_client.TakeClientEnd(),
      std::move(suspend_blocker_endpoints.value().server),
      dispatcher);
}

ActivityGovernorLeaseProvider::ActivityGovernorLeaseProvider(
    fidl::ClientEnd<ActivityGovernor> activity_governor_client,
    fidl::ServerEnd<SuspendBlocker> suspend_blocker_server,
    async_dispatcher_t* dispatcher)
    : governor_(std::move(activity_governor_client)),
      binding_ref_(fidl::BindServer(
          dispatcher, std::move(suspend_blocker_server), this)) {}

pw::Result<pw::bluetooth_sapphire::Lease>
ActivityGovernorLeaseProvider::Acquire(const char*) {
  ref_count_++;

  if (!token_ && state_ == State::kSuspending) {
    AcquireWakeLease();
  }

  auto lease_dropped_cb = [self = weak_ptr_factory_.GetWeakPtr()]() {
    if (self.is_alive()) {
      self->OnLeaseDropped();
    }
  };

  return pw::bluetooth_sapphire::Lease(std::move(lease_dropped_cb));
}

void ActivityGovernorLeaseProvider::OnLeaseDropped() {
  PW_DCHECK_UINT_NE(ref_count_, 0u);
  ref_count_--;
  if (ref_count_ == 0) {
    token_.reset();
  }
}

void ActivityGovernorLeaseProvider::AcquireWakeLease() {
  PW_DCHECK(!token_);
  fidl::Request<ActivityGovernor::AcquireWakeLease> request;
  request.name() = kLeaseName;

  fidl::Result<ActivityGovernor::AcquireWakeLease> result =
      governor_->AcquireWakeLease(request);
  if (result.is_error()) {
    bt_log(ERROR,
           "fidl",
           "Failed to acquire wake lease: %s",
           result.error_value().FormatDescription().c_str());
    return;
  }
  token_ = std::move(result->token());
}

void ActivityGovernorLeaseProvider::BeforeSuspend(
    BeforeSuspendCompleter::Sync& completer) {
  state_ = State::kSuspending;
  if (!token_ && ref_count_ != 0) {
    AcquireWakeLease();
  }
  completer.Reply();
}

void ActivityGovernorLeaseProvider::AfterResume(
    AfterResumeCompleter::Sync& completer) {
  state_ = State::kResumed;
  completer.Reply();
}

}  // namespace bthost
