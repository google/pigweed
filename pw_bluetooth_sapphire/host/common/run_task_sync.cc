// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_task_sync.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <zircon/assert.h>

#include <condition_variable>
#include <mutex>

namespace bt {

void RunTaskSync(fit::closure callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(callback);

  // TODO(armansito) This check is risky. async_get_default_dispatcher() could
  // return a dispatcher that goes to another thread. We don't have any current
  // instances of a multi threaded dispatcher but we could.
  if (dispatcher == async_get_default_dispatcher()) {
    callback();
    return;
  }

  std::mutex mtx;
  std::condition_variable cv;
  bool done = false;

  async::PostTask(dispatcher, [callback = std::move(callback), &mtx, &cv, &done] {
    callback();

    {
      std::lock_guard<std::mutex> lock(mtx);
      done = true;
    }

    cv.notify_one();
  });

  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock, [&done] { return done; });
}

}  // namespace bt
