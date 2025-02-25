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

namespace bthost {

namespace {
constexpr const char* kLeaseName = "bt-host";
}

ActivityGovernorLeaseProvider::ActivityGovernorLeaseProvider(
    fidl::ClientEnd<fuchsia_power_system::ActivityGovernor> client_end)
    : governor_(std::move(client_end)) {}

pw::Result<pw::bluetooth_sapphire::Lease>
ActivityGovernorLeaseProvider::Acquire(const char*) {
  if (token_) {
    ref_count_++;
    return pw::bluetooth_sapphire::Lease(
        [self = weak_ptr_factory_.GetWeakPtr()]() {
          if (self.is_alive()) {
            self->OnLeaseDropped();
          }
        });
  }

  ::fidl::Request<::fuchsia_power_system::ActivityGovernor::AcquireWakeLease>
      request;
  request.name() = kLeaseName;

  ::fidl::Result<::fuchsia_power_system::ActivityGovernor::AcquireWakeLease>
      result = governor_->AcquireWakeLease(request);
  if (result.is_error()) {
    return pw::Status::Unavailable();
  }

  token_ = std::move(result->token());
  ref_count_++;
  return pw::bluetooth_sapphire::Lease(
      [self = weak_ptr_factory_.GetWeakPtr()]() {
        if (self.is_alive()) {
          self->OnLeaseDropped();
        }
      });
}

void ActivityGovernorLeaseProvider::OnLeaseDropped() {
  PW_DCHECK_UINT_NE(ref_count_, 0u);
  ref_count_--;
  if (ref_count_ == 0) {
    token_->reset();
  }
}

}  // namespace bthost
