// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/common/slab_allocator.h"

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/slab_buffer.h"

namespace bt {

MutableByteBufferPtr NewBuffer(size_t size) {
  // TODO(fxbug.dev/106841): Use Pigweed's slab allocator
  return std::make_unique<DynamicByteBuffer>(size);
}

}  // namespace bt
