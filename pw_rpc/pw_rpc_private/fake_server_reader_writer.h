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

#include <cstring>

#include "pw_bytes/span.h"
#include "pw_rpc/internal/call.h"
#include "pw_rpc/method_type.h"

namespace pw::rpc::internal::test {

// Fake server reader/writer classes for testing use. These also serve as a
// model for how the RPC implementations (raw, pwpb, Nanopb) structure their
// reader/writer classes.
//
// Readers/writers use an unusual inheritance hierarchy. Rather than having the
// ServerReaderWriter inherit from both the Reader and Writer classes, the
// readers and writers inherit from it, but hide the unsupported functionality.
// A ReaderWriter defines conversions to Reader and Writer, so it acts as if it
// inherited from both. This approach is unusual but necessary to have all
// classes use a single IntrusiveList::Item base and to avoid virtual methods or
// virtual inheritance.
//
// Responder's public API is intended for rpc::Server, so hide the public
// methods with private inheritance.
class FakeServerReaderWriter : private internal::Responder {
 public:
  constexpr FakeServerReaderWriter()
      : FakeServerReaderWriter(MethodType::kBidirectionalStreaming) {}

  // On a real reader/writer, this constructor would not be exposed.
  FakeServerReaderWriter(CallContext& context,
                         MethodType type = MethodType::kBidirectionalStreaming)
      : Responder(context, type) {}

  FakeServerReaderWriter(FakeServerReaderWriter&&) = default;
  FakeServerReaderWriter& operator=(FakeServerReaderWriter&&) = default;

  // Pull in protected functions from the hidden Responder base as needed.
  using Responder::open;
  using Responder::set_on_client_stream_end;
  using Responder::set_on_error;
  using Responder::set_on_next;

  Status Finish(Status status = OkStatus()) {
    return CloseAndSendResponse(status);
  }

  Status Write(ConstByteSpan response) {
    std::span buffer = AcquirePayloadBuffer();
    std::memcpy(buffer.data(),
                response.data(),
                std::min(buffer.size(), response.size()));
    return SendPayloadBufferClientStream(buffer.first(response.size()));
  }

  // Expose a few additional methods for test use.
  Responder& as_responder() { return *this; }
  ByteSpan PayloadBuffer() { return AcquirePayloadBuffer(); }
  const Channel::OutputBuffer& output_buffer() { return buffer(); }

 protected:
  constexpr FakeServerReaderWriter(MethodType type)
      : internal::Responder(type) {}
};

class FakeServerWriter : private FakeServerReaderWriter {
 public:
  constexpr FakeServerWriter()
      : FakeServerReaderWriter(MethodType::kServerStreaming) {}
  FakeServerWriter(CallContext& context)
      : FakeServerReaderWriter(context, MethodType::kServerStreaming) {}
  FakeServerWriter(FakeServerWriter&&) = default;

  // Common reader/writer functions.
  using FakeServerReaderWriter::Finish;
  using FakeServerReaderWriter::open;
  using FakeServerReaderWriter::set_on_error;
  using FakeServerReaderWriter::Write;

  // Functions for test use.
  using FakeServerReaderWriter::as_responder;
  using FakeServerReaderWriter::output_buffer;
  using FakeServerReaderWriter::PayloadBuffer;
};

class FakeServerReader : private FakeServerReaderWriter {
 public:
  constexpr FakeServerReader()
      : FakeServerReaderWriter(MethodType::kClientStreaming) {}

  FakeServerReader(CallContext& context)
      : FakeServerReaderWriter(context, MethodType::kClientStreaming) {}

  FakeServerReader(FakeServerReader&&) = default;

  using FakeServerReaderWriter::as_responder;
  using FakeServerReaderWriter::open;

  // Functions for test use.
  using FakeServerReaderWriter::PayloadBuffer;
};

}  // namespace pw::rpc::internal::test
