// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RUN_OR_POST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RUN_OR_POST_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

namespace bt {

// Runs |task|. Posts it on |dispatcher| if dispatcher is not null.
void RunOrPost(fit::closure task, async_dispatcher_t* dispatcher);

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_RUN_OR_POST_H_
