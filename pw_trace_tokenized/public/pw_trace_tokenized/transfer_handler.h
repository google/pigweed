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
#pragma once

#include "pw_transfer/handler.h"

namespace pw::trace {

/// @module{pw_trace_tokenized}

/// A ReadOnlyHandler that transfers trace buffer data via pw_transfer.
///
/// To use, create a handler object and register with
/// pw::transfer::TransferService::RegisterHandler
///
/// Stop tracing before transfer if the reader object passed
/// to the constructor does not provide synchronization between
/// concurrent read and write on the trace buffer.
class TraceTransferHandler : public transfer::ReadOnlyHandler {
 public:
  TraceTransferHandler(uint32_t id, stream::Reader& reader)
      : ReadOnlyHandler(id, reader) {}
};

/// A reader that reads data from trace buffer.
///
/// The implementation does not provide synchronization between
/// concurrent read and write on the trace buffer.
class TraceBufferReader : public stream::NonSeekableReader {
 public:
  constexpr TraceBufferReader() = default;
  StatusWithSize DoRead(ByteSpan dest) final;
};

TraceBufferReader& GetTraceBufferReader();

}  // namespace pw::trace
