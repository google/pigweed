// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "test_helpers.h"

#include <zircon/syscalls.h>
#include <zircon/types.h>

namespace bt {

bool IsChannelPeerClosed(const zx::channel& channel) {
  zx_signals_t ignored;
  return ZX_OK == channel.wait_one(/*signals=*/ZX_CHANNEL_PEER_CLOSED,
                                   /*deadline=*/zx::time(ZX_TIME_INFINITE_PAST), &ignored);
}

}  // namespace bt
