// Copyright 2025 The Pigweed Authors
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

#include "pw_trace_tokenized/transfer_handler.h"

#include <climits>

#include "pw_log/log.h"
#include "pw_trace_tokenized/trace_buffer.h"

namespace pw::trace {
namespace {

TraceBufferReader trace_buffer_reader;

}  // namespace

TraceBufferReader& GetTraceBufferReader() { return trace_buffer_reader; }

StatusWithSize TraceBufferReader::DoRead(ByteSpan dest) {
  pw::ring_buffer::PrefixedEntryRingBuffer* trace_buffer =
      pw::trace::GetBuffer();
  size_t size = 0;
  size_t start = 0;
  Status sts;

  PW_LOG_DEBUG("Entry count is: %zu", trace_buffer->EntryCount());
  while (start + 1 < dest.size()) {
    sts = trace_buffer->PeekFront(dest.subspan(start + 1), &size);
    if (!sts.ok()) {
      break;
    }
    // Max size of a trace entry is not expected to be bigger than
    // 256 bytes. The value of size should always fit into 1 byte.
    if (size > UCHAR_MAX) {
      PW_LOG_ERROR("Unexpected entry size: %zu", size);
      sts = pw::Status::OutOfRange();
      break;
    }
    dest[start] = static_cast<std::byte>(size);
    start += size + 1;
    trace_buffer->PopFront().IgnoreError();
  }

  if (start > 0) {
    return StatusWithSize(start);
  }
  return StatusWithSize(sts, 0);
}
}  // namespace pw::trace
