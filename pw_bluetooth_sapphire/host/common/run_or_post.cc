// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_or_post.h"

#include <lib/async/cpp/task.h>
#include <zircon/assert.h>

namespace btlib {
namespace common {

void RunOrPost(fit::closure task, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(task);

  if (!dispatcher) {
    task();
    return;
  }

  async::PostTask(dispatcher, std::move(task));
}

}  // namespace common
}  // namespace btlib
