// Copyright 2021 The Pigweed Authors
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
#pragma once

#include "pw_bytes/span.h"

namespace pw::transfer::internal {

// Stores deferred write chunk data for consumption in a work queue context.
//
// To avoid blocking an RPC thread, transfer data in receive transfer is not
// written directly to a stream::Writer from the RPC callback. Instead, it is
// copied into this buffer and later drained by a job in a work queue. This
// buffer must be locked when it is written to, and unlocked when drained.
class ChunkDataBuffer {
 public:
  constexpr ChunkDataBuffer(ByteSpan buffer)
      : buffer_(buffer), size_(0), last_chunk_(false) {}

  constexpr std::byte* data() const { return buffer_.data(); }

  constexpr size_t size() const { return size_; }
  constexpr size_t max_size() const { return buffer_.size(); }

  constexpr bool empty() const { return size() == 0u; }

  constexpr bool last_chunk() const { return last_chunk_; }

  void Write(ConstByteSpan data, bool last_chunk);

 private:
  // TODO(frolv): This should be locked for use between an RPC thread and work
  // queue.
  ByteSpan buffer_;
  size_t size_;
  bool last_chunk_;
};

}  // namespace pw::transfer::internal
