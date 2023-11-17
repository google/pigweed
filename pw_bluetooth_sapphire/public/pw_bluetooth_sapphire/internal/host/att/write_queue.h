// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_WRITE_QUEUE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_WRITE_QUEUE_H_

#include <queue>

#include "pw_bluetooth_sapphire/internal/host/att/att.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"

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
