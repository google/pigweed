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

#include <chrono>
#include <functional>
#include <thread>

#include "chre/core/event_loop_manager.h"
#include "chre/core/init.h"
#include "chre/platform/memory.h"
#include "chre/platform/system_timer.h"
#include "chre/util/fixed_size_blocking_queue.h"
#include "chre/util/memory.h"
#include "chre/util/non_copyable.h"
#include "chre/util/singleton.h"
#include "chre/util/time.h"
#include "chre_api/chre/version.h"
#include "gtest/gtest.h"
#include "test_base.h"
#include "test_util.h"

namespace chre {

void TestBase::SetUp() {
  TestEventQueueSingleton::init();
  chre::init();
  EventLoopManagerSingleton::get()->lateInit();

  mChreThread = std::thread(
      []() { EventLoopManagerSingleton::get()->getEventLoop().run(); });

  auto callback = [](void*) {
    LOGE("Test timed out ...");
    TestEventQueueSingleton::get()->pushEvent(
        CHRE_EVENT_SIMULATION_TEST_TIMEOUT);
  };

  ASSERT_TRUE(mSystemTimer.init());
  ASSERT_TRUE(mSystemTimer.set(
      callback, nullptr /*data*/, Nanoseconds(getTimeoutNs())));
}

void TestBase::TearDown() {
  mSystemTimer.cancel();
  TestEventQueueSingleton::get()->flush();
  EventLoopManagerSingleton::get()->getEventLoop().stop();
  mChreThread.join();
  chre::deinit();
  TestEventQueueSingleton::deinit();
  deleteNanoappInfos();
}

template class Singleton<TestEventQueue>;

TEST_F(TestBase, CanLoadAndStartSingleNanoapp) {
  constexpr uint64_t kAppId = 0x0123456789abcdef;
  constexpr uint32_t kAppVersion = 0;
  constexpr uint32_t kAppPerms = 0;

  UniquePtr<Nanoapp> nanoapp = createStaticNanoapp("Test nanoapp",
                                                   kAppId,
                                                   kAppVersion,
                                                   kAppPerms,
                                                   defaultNanoappStart,
                                                   defaultNanoappHandleEvent,
                                                   defaultNanoappEnd);

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::FinishLoadingNanoapp,
      std::move(nanoapp),
      testFinishLoadingNanoappCallback);
  waitForEvent(CHRE_EVENT_SIMULATION_TEST_NANOAPP_LOADED);
}

TEST_F(TestBase, CanLoadAndStartMultipleNanoapps) {
  constexpr uint64_t kAppId1 = 0x123;
  constexpr uint64_t kAppId2 = 0x456;
  constexpr uint32_t kAppVersion = 0;
  constexpr uint32_t kAppPerms = 0;
  loadNanoapp("Test nanoapp",
              kAppId1,
              kAppVersion,
              kAppPerms,
              defaultNanoappStart,
              defaultNanoappHandleEvent,
              defaultNanoappEnd);

  loadNanoapp("Test nanoapp",
              kAppId2,
              kAppVersion,
              kAppPerms,
              defaultNanoappStart,
              defaultNanoappHandleEvent,
              defaultNanoappEnd);

  uint16_t id1;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(kAppId1, &id1));
  uint16_t id2;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(kAppId2, &id2));

  EXPECT_NE(id1, id2);
}
}  // namespace chre
