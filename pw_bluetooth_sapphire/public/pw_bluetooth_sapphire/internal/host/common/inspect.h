// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECT_H_

#include "pw_bluetooth_sapphire/config.h"

#ifdef NINSPECT
#include "pw_bluetooth_sapphire/internal/host/common/fake_inspect.h"
#else
#include <lib/inspect/cpp/inspect.h>
#endif  // NINSPECT

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_INSPECT_H_
