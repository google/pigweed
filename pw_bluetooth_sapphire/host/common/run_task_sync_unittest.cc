// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/run_task_sync.h"

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#include <lib/zx/time.h>

#include "gtest/gtest.h"

namespace bt {
namespace {

TEST(RunTaskSyncTest, RunTaskSync) {
  constexpr zx::duration kSleepTime = zx::msec(10);
  constexpr int kLoopCount = 10;

  async::Loop loop(&kAsyncLoopConfigNoAttachToCurrentThread);
  loop.StartThread("RunTaskSyncTest thread");
  auto dispatcher = loop.dispatcher();

  for (int i = 0; i < kLoopCount; ++i) {
    bool callback_run = false;
    auto cb = [&callback_run, kSleepTime] {
      zx::nanosleep(zx::deadline_after(kSleepTime));
      callback_run = true;
    };

    RunTaskSync(cb, dispatcher);
    EXPECT_TRUE(callback_run);
  }

  async::PostTask(dispatcher, [&loop] { loop.Quit(); });
  loop.JoinThreads();
}

}  // namespace
}  // namespace bt
