// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

// Prevent "undefined symbol: __zircon_driver_rec__" error.
BT_DECLARE_FAKE_DRIVER();

// Entry point for libFuzzer that switches logging to printf output with lower verbosity.
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  bt::UsePrintf(bt::LogSeverity::ERROR);
  return 0;
}
