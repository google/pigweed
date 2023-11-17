// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_H_

#include "pw_bluetooth_sapphire/config.h"

#ifdef NINSPECT
// Fixes clang "no namespace named 'testing' in namespace 'inspect' errors"
namespace inspect::testing {}
#else
#include <lib/inspect/testing/cpp/inspect.h>
#endif

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_H_
