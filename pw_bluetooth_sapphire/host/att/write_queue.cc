// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "write_queue.h"

namespace bt {
namespace att {

QueuedWrite::QueuedWrite(Handle handle, uint16_t offset,
                         const ByteBuffer& value)
    : handle_(handle), offset_(offset), value_(value) {}

}  // namespace att
}  // namespace bt
