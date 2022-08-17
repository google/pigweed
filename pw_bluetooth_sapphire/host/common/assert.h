// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_

#include <zircon/assert.h>

#define BT_PANIC(args...) ZX_PANIC(args)
#define BT_ASSERT(x) ZX_ASSERT(x)
#define BT_ASSERT_MSG(x, msg, args...) ZX_ASSERT_MSG(x, msg, ##args)
#define BT_DEBUG_ASSERT(x) ZX_DEBUG_ASSERT(x)
#define BT_DEBUG_ASSERT_MSG(x, msg, args...) ZX_DEBUG_ASSERT_MSG(x, msg, ##args)

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_ASSERT_H_
