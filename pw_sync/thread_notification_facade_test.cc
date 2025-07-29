// Copyright 2020 The Pigweed Authors
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

#include "pw_sync/test/threaded_testing.h"
#include "pw_sync/thread_notification.h"
#include "pw_unit_test/framework.h"

namespace pw::sync {
namespace {

// Test fixture used to release the notification.
class ThreadNotificationTest : public test::OptionallyThreadedTest {
 protected:
  void ReleaseTwice(ThreadNotification& notification) {
    notification_ = &notification;
    RunOnce();
  }

 private:
  void DoStop() override {
    // Multiple releases are the same as one.
    notification_->release();
    notification_->release();
  }

  ThreadNotification* notification_ = nullptr;
};

TEST_F(ThreadNotificationTest, EmptyInitialState) {
  ThreadNotification notification;
  EXPECT_FALSE(notification.try_acquire());
}

TEST_F(ThreadNotificationTest, Release) {
  ThreadNotification notification;
  ReleaseTwice(notification);
  notification.acquire();
  // Ensure it fails when empty.
  EXPECT_FALSE(notification.try_acquire());
}

ThreadNotification empty_initial_notification;
TEST_F(ThreadNotificationTest, EmptyInitialStateStatic) {
  EXPECT_FALSE(empty_initial_notification.try_acquire());
}

ThreadNotification raise_notification;
TEST_F(ThreadNotificationTest, ReleaseStatic) {
  ReleaseTwice(raise_notification);
  raise_notification.acquire();
  // Ensure it fails when empty.
  EXPECT_FALSE(raise_notification.try_acquire());
}

}  // namespace
}  // namespace pw::sync
