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

#include "pw_bluetooth_sapphire/lease.h"

namespace pw::bluetooth_sapphire {

/// @module{pw_bluetooth_sapphire}

/// Interface for Sapphire to interact with the system's power management.
class PowerDelegate {
 public:
  virtual ~PowerDelegate() = default;

  /// Provides leases that ensure the scheduler keeps scheduling Bluetooth
  /// tasks. For example, a lease will be taken while processing packets or
  /// during pairing.
  virtual LeaseProvider& SchedulerLeaseProvider() = 0;
};

}  // namespace pw::bluetooth_sapphire
