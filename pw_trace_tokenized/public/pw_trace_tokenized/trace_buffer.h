// Copyright 2020 The Pigweed Authors
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
//==============================================================================
//
// This file provides an optional trace buffer which can be used with the
#pragma once

#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"
#include "pw_trace_tokenized/trace_tokenized.h"
#include "pw_varint/varint.h"

// Configurable options
// The buffer is automatically registered at boot if the buffer size is not 0.
#ifndef PW_TRACE_BUFFER_SIZE_BYTES
#define PW_TRACE_BUFFER_SIZE_BYTES 256
#endif  // PW_TRACE_BUFFER_SIZE_BYTES

// PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES indicates the maximum size any
// individual encoded trace event could be. This is used internally to buffer up
// a sample before saving into the buffer.
#ifndef PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES
// The below calaculation is provided to help determine a suitable value, using
// the max data size bytes.
#ifndef PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES
#define PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES (32)
#endif  // PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES

#define PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES                                 \
  (pw::varint::kMaxVarintSizeBytes) +     /* worst case delta time varint */ \
      (sizeof(uint32_t)) +                /* trace token size */             \
      (pw::varint::kMaxVarintSizeBytes) + /* worst case trace id varint */   \
      PW_TRACE_BUFFER_MAX_DATA_SIZE_BYTES
#endif  // PW_TRACE_BUFFER_MAX_BLOCK_SIZE_BYTES

namespace pw {
namespace trace {

// pw_TraceClearBuffer resets the trace buffer, and all data currently stored
// in the buffer is lost.
void ClearBuffer();

// Get the ring buffer which contains the data.
pw::ring_buffer::PrefixedEntryRingBuffer* GetBuffer();

}  // namespace trace
}  // namespace pw
