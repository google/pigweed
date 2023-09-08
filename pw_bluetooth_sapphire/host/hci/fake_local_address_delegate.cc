// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_local_address_delegate.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

namespace bt::hci {

void FakeLocalAddressDelegate::EnsureLocalAddress(AddressCallback callback) {
  BT_DEBUG_ASSERT(callback);
  if (!async_) {
    callback(local_address_);
    return;
  }
  heap_dispatcher_.Post([callback = std::move(callback), addr = local_address_](
                            pw::async::Context /*ctx*/, pw::Status status) {
    if (status.ok()) {
      callback(addr);
    }
  });
}

}  // namespace bt::hci
