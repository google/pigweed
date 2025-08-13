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

#include "pw_sync/no_lock.h"

#include <mutex>
#include <type_traits>

#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_unit_test/framework.h"

namespace pw::sync {
namespace {

// A simple class that uses lock annotations to verify that a lock type works
// correctly with thread-safety analysis.
template <typename LockType>
class TestLockedData {
 public:
  void SetValue(int value) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    value_ = value;
  }

  int GetValue() PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    return value_;
  }

 private:
  LockType lock_;
  int value_ PW_GUARDED_BY(lock_) = 0;
};

TEST(NoLock, CompilesWithLockAnnotations) {
  TestLockedData<NoLock> data;
  data.SetValue(42);
  EXPECT_EQ(data.GetValue(), 42);
}

TEST(MaybeLock, SelectsNoLockWhenFalse) {
  using Lock = MaybeLock<false, Mutex>;
  static_assert(std::is_same_v<Lock, NoLock>);

  TestLockedData<Lock> data;
  data.SetValue(123);
  EXPECT_EQ(data.GetValue(), 123);
}

TEST(MaybeLock, SelectsLockTypeWhenTrue) {
  using Lock = MaybeLock<true, Mutex>;
  static_assert(std::is_same_v<Lock, Mutex>);

  TestLockedData<Lock> data;
  data.SetValue(456);
  EXPECT_EQ(data.GetValue(), 456);
}

}  // namespace
}  // namespace pw::sync
