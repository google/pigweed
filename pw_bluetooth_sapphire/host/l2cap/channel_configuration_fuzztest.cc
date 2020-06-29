// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

// Prevent "undefined symbol: __zircon_driver_rec__" error.
BT_DECLARE_FAKE_DRIVER();

namespace bt {
namespace l2cap {
namespace internal {

void fuzz(const uint8_t* data, size_t size) {
  DynamicByteBuffer buf(size);
  memcpy(buf.mutable_data(), data, size);
  ChannelConfiguration config;
  bool _result = config.ReadOptions(buf);
  // unused.
  (void)_result;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::l2cap::internal::fuzz(data, size);
  return 0;
}
