// Copyright 2023 The Pigweed Authors
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

#include "pw_sync/lock_testing.h"

#include "pw_assert/check.h"

namespace pw::sync::test {

void FakeBasicLockable::DoLockOperation(Operation operation) {
  switch (operation) {
    case Operation::kLock:
      PW_CHECK(!locked_, "Recursive lock detected");
      locked_ = true;
      return;

    case Operation::kUnlock:
    default:
      PW_CHECK(locked_, "Unlock while unlocked detected");
      locked_ = false;
      return;
  }
}

bool FakeLockable::try_lock() {
  if (locked()) {
    return false;
  }
  locked_ = true;
  return true;
}

}  // namespace pw::sync::test
