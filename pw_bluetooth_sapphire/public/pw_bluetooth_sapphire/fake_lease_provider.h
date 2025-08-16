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

namespace pw::bluetooth_sapphire::testing {

/// @module{pw_bluetooth_sapphire}

/// A fake LeaseProvider used for dependency injection in unit tests.
class FakeLeaseProvider final : public LeaseProvider {
 public:
  FakeLeaseProvider() = default;
  ~FakeLeaseProvider() override = default;
  FakeLeaseProvider(FakeLeaseProvider&&) = delete;
  FakeLeaseProvider& operator=(FakeLeaseProvider&&) = delete;

  Result<Lease> Acquire(PW_SAPPHIRE_LEASE_TOKEN_TYPE) override {
    if (!status_.ok()) {
      return status_;
    }
    lease_count_++;
    return Lease([this]() { lease_count_--; });
  }

  /// Returns the number of active leases.
  uint16_t lease_count() const { return lease_count_; }

  /// Sets the status to return from the `Acquire` method.
  void set_acquire_status(Status status) { status_ = status; }

 private:
  uint16_t lease_count_ = 0;
  Status status_ = PW_STATUS_OK;
};

}  // namespace pw::bluetooth_sapphire::testing
