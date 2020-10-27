// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_WRITE_QUEUE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_WRITE_QUEUE_H_

#include <fbl/macros.h>

#include <queue>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt::att {

// Represents a singe write operation queued for atomic submission by an ATT
// protocol write method
class QueuedWrite {
 public:
  QueuedWrite() = default;
  ~QueuedWrite() = default;

  // Constructs a write request by copying the contents of |value|.
  QueuedWrite(Handle handle, uint16_t offset, const ByteBuffer& value);

  // Allow move operations.
  QueuedWrite(QueuedWrite&&) = default;
  QueuedWrite& operator=(QueuedWrite&&) = default;

  Handle handle() const { return handle_; }
  uint16_t offset() const { return offset_; }
  const ByteBuffer& value() const { return value_; }

 private:
  Handle handle_;
  uint16_t offset_;
  DynamicByteBuffer value_;
};

// Represents a prepare queue used to handle the ATT Prepare Write and Execute
// Write requests.
using PrepareWriteQueue = std::queue<QueuedWrite>;

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_WRITE_QUEUE_H_
