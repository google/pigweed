// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_

#include "pw_assert/check.h"

#define BT_PANIC(msg, ...) PW_CRASH(msg, ##__VA_ARGS__)
#define BT_ASSERT(x) PW_CHECK(x)
#define BT_ASSERT_MSG(x, msg, ...) PW_CHECK(x, msg, ##__VA_ARGS__)
#define BT_DEBUG_ASSERT(x) PW_DCHECK(x)
#define BT_DEBUG_ASSERT_MSG(x, msg, ...) PW_DCHECK(x, msg, ##__VA_ARGS__)

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_
