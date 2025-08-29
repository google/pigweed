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

#include <type_traits>

#include "pw_sync/lock_annotations.h"

namespace pw::sync {

/// @module{pw_sync}

/// `NoLock` is a no-op lock that satisfies the C++ named requirements for
/// `BasicLockable` but performs no synchronization. This can be used for code
/// that is conditionally thread-safe.
class PW_LOCKABLE("pw::sync::NoLock") NoLock {
 public:
  constexpr void lock() PW_EXCLUSIVE_LOCK_FUNCTION() {}
  constexpr void unlock() PW_UNLOCK_FUNCTION() {}
  [[nodiscard]] bool try_lock() PW_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return true;
  }
};

/// `MaybeLock` is a helper that selects between a real lock type and `NoLock`
/// based on a boolean template parameter.
///
/// @tparam kEnableLocking  If true, `MaybeLock` is an alias for `LockType`. If
///                         false, `MaybeLock` is an alias for `NoLock`.
/// @tparam LockType        The lock type to use when locking is enabled.
template <bool kEnableLocking, typename LockType>
using MaybeLock = std::conditional_t<kEnableLocking, LockType, NoLock>;

/// @}

}  // namespace pw::sync
