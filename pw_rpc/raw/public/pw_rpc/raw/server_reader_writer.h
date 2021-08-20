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

// This file defines the ServerReaderWriter, ServerReader, and ServerWriter
// classes for the raw RPC interface. These classes are used for bidirectional,
// client, and server streaming RPCs.
#pragma once

#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/responder.h"
#include "pw_rpc/server.h"

namespace pw::rpc {
namespace internal {

// Forward declarations for internal classes needed in friend statements.
class RawMethod;

namespace test {

template <typename, uint32_t>
class InvocationContext;

}  // namespace test
}  // namespace internal

class RawServerReader;
class RawServerWriter;

// The RawServerReaderWriter is used to send and receive messages in a raw
// bidirectional streaming RPC.
class RawServerReaderWriter : private internal::Responder {
 public:
  constexpr RawServerReaderWriter()
      : RawServerReaderWriter(MethodType::kBidirectionalStreaming) {}

  RawServerReaderWriter(RawServerReaderWriter&&) = default;
  RawServerReaderWriter& operator=(RawServerReaderWriter&&) = default;

  using internal::Responder::open;

  using internal::Responder::channel_id;

  // Functions for setting the callbacks.
  using internal::Responder::set_on_client_stream_end;
  using internal::Responder::set_on_error;
  using internal::Responder::set_on_next;

  // Returns a buffer in which a response payload can be built.
  ByteSpan PayloadBuffer() { return AcquirePayloadBuffer(); }

  // Releases a buffer acquired from PayloadBuffer() without sending any data.
  void ReleaseBuffer() { ReleasePayloadBuffer(); }

  // Sends a response packet with the given raw payload. The payload can either
  // be in the buffer previously acquired from PayloadBuffer(), or an arbitrary
  // external buffer.
  Status Write(ConstByteSpan response);

  Status Finish(Status status = OkStatus()) {
    return CloseAndSendResponse(status);
  }

 protected:
  // Constructor for derived classes to use.
  constexpr RawServerReaderWriter(MethodType type)
      : internal::Responder(type) {}

  RawServerReaderWriter(const internal::ServerCall& call,
                        MethodType type = MethodType::kBidirectionalStreaming)
      : internal::Responder(call, type) {}

  using internal::Responder::CloseAndSendResponse;

 private:
  friend class internal::RawMethod;

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;
};

// The RawServerReader is used to receive messages and send a response in a
// raw client streaming RPC.
class RawServerReader : private RawServerReaderWriter {
 public:
  constexpr RawServerReader()
      : RawServerReaderWriter(MethodType::kClientStreaming) {}

  RawServerReader(RawServerReader&&) = default;
  RawServerReader& operator=(RawServerReader&&) = default;

  using RawServerReaderWriter::open;

  using RawServerReaderWriter::channel_id;

  using RawServerReaderWriter::set_on_client_stream_end;
  using RawServerReaderWriter::set_on_error;
  using RawServerReaderWriter::set_on_next;

  using RawServerReaderWriter::PayloadBuffer;

  Status Finish(ConstByteSpan response, Status status = OkStatus()) {
    return CloseAndSendResponse(response, status);
  }

 private:
  friend class internal::RawMethod;  // Needed for conversions from ReaderWriter

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;

  RawServerReader(const internal::ServerCall& call)
      : RawServerReaderWriter(call, MethodType::kClientStreaming) {}
};

// The RawServerWriter is used to send responses in a raw server streaming RPC.
class RawServerWriter : private RawServerReaderWriter {
 public:
  constexpr RawServerWriter()
      : RawServerReaderWriter(MethodType::kServerStreaming) {}

  RawServerWriter(RawServerWriter&&) = default;
  RawServerWriter& operator=(RawServerWriter&&) = default;

  using RawServerReaderWriter::open;

  using RawServerReaderWriter::channel_id;

  using RawServerReaderWriter::set_on_error;

  using RawServerReaderWriter::Finish;
  using RawServerReaderWriter::PayloadBuffer;
  using RawServerReaderWriter::ReleaseBuffer;
  using RawServerReaderWriter::Write;

 private:
  friend class RawServerReaderWriter;  // Needed for conversions.

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;

  friend class internal::RawMethod;

  RawServerWriter(const internal::ServerCall& call)
      : RawServerReaderWriter(call, MethodType::kServerStreaming) {}
};

}  // namespace pw::rpc
