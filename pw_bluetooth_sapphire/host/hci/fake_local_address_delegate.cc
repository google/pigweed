// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_local_address_delegate.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

namespace bt::hci {

void FakeLocalAddressDelegate::EnsureLocalAddress(AddressCallback callback) {
  ZX_DEBUG_ASSERT(callback);
  if (!async_) {
    callback(local_address_);
    return;
  }
  async::PostTask(async_get_default_dispatcher(),
                  [callback = std::move(callback), addr = local_address_] { callback(addr); });
}

}  // namespace bt::hci
