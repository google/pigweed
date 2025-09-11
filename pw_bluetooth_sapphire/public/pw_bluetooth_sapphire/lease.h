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

#include "pw_bluetooth_sapphire/config.h"
#include "pw_function/function.h"
#include "pw_result/result.h"

/// Macro for obtaining a lease that supports tokenization properly.
/// @param lease_provider A `LeaseProvider&`
/// @param name The name of the lease (string literal)
#define PW_SAPPHIRE_ACQUIRE_LEASE(lease_provider, name) \
  lease_provider.Acquire(PW_SAPPHIRE_LEASE_TOKEN_EXPR(name))

namespace pw::bluetooth_sapphire {

/// @module{pw_bluetooth_sapphire}

/// A handle representing an active lease. The lease is released on
/// destruction.
class Lease final {
 public:
  Lease() = default;
  Lease(pw::Function<void()> unlock_fn) : unlock_fn_(std::move(unlock_fn)) {}
  Lease(Lease&&) = default;
  Lease& operator=(Lease&&) = default;
  Lease(const Lease&) = delete;
  Lease& operator=(const Lease&) = delete;

  ~Lease() {
    if (unlock_fn_) {
      unlock_fn_();
    }
  }

 private:
  pw::Function<void()> unlock_fn_ = nullptr;
};

/// Interface for acquiring leases. This interface is what backends
/// implement.
class LeaseProvider {
 public:
  virtual ~LeaseProvider() = default;

  /// Try to acquire a lease.
  /// Prefer to use `PW_SAPPHIRE_ACQUIRE_LEASE` instead.
  /// @param name will be either a `const char*` or a `uint32_t` depending on
  /// whether tokenization is enabled.
  ///
  /// @returns @Result{the lease}
  /// * @UNAVAILABLE: A lease could not be created.
  /// * @INVALID_ARGUMENT: The name was invalid (e.g. empty).
  virtual Result<Lease> Acquire(PW_SAPPHIRE_LEASE_TOKEN_TYPE name) = 0;
};

}  // namespace pw::bluetooth_sapphire
