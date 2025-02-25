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

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/lease.h"

namespace bthost {

// This class is not thread safe.
// Leases vended by ActivityGovernorLeaseProvider will only be valid as long as
// ActivityGovernorLeaseProvider is alive.
class ActivityGovernorLeaseProvider final
    : public pw::bluetooth_sapphire::LeaseProvider {
 public:
  ActivityGovernorLeaseProvider(
      fidl::ClientEnd<::fuchsia_power_system::ActivityGovernor> client_end);
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ActivityGovernorLeaseProvider);

  ~ActivityGovernorLeaseProvider() override = default;
  pw::Result<pw::bluetooth_sapphire::Lease> Acquire(const char* name) override;

 private:
  void OnLeaseDropped();

  fidl::SyncClient<fuchsia_power_system::ActivityGovernor> governor_;
  std::optional<::fuchsia_power_system::LeaseToken> token_;
  uint16_t ref_count_ = 0;
  WeakSelf<ActivityGovernorLeaseProvider> weak_ptr_factory_{this};
};

}  // namespace bthost
