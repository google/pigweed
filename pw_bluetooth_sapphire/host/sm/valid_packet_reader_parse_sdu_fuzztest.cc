// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"

namespace bt::sm {

void fuzz(const uint8_t* data, size_t size) {
  DynamicByteBuffer buf(size);
  memcpy(buf.mutable_data(), data, size);
  ByteBufferPtr buf_ptr = std::make_unique<DynamicByteBuffer>(buf);
  [[maybe_unused]] fpromise::result<ValidPacketReader, ErrorCode> result =
      ValidPacketReader::ParseSdu(buf_ptr);
}

}  // namespace bt::sm

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bt::sm::fuzz(data, size);
  return 0;
}
