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
// classes for the Nanopb RPC interface. These classes are used for
// bidirectional, client, and server streaming RPCs.
#pragma once

#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/responder.h"
#include "pw_rpc/server.h"

namespace pw::rpc {
namespace internal {

// Forward declarations for internal classes needed in friend statements.
class NanopbMethod;

namespace test {

template <typename, uint32_t>
class InvocationContext;

}  // namespace test

// Non-templated base so the methods are instantiated only once.
class GenericNanopbResponder : public internal::Responder {
 public:
  constexpr GenericNanopbResponder(MethodType type)
      : internal::Responder(type) {}

  GenericNanopbResponder(const ServerCall& call, MethodType type)
      : internal::Responder(call, type) {}

 protected:
  Status SendClientStream(const void* response) {
    return SendClientStreamOrResponse(response, nullptr);
  }

  Status SendResponse(const void* response, Status status) {
    return SendClientStreamOrResponse(response, &status);
  }

  void DecodeRequest(ConstByteSpan payload, void* request_struct) const;

 private:
  Status SendClientStreamOrResponse(const void* response, Status* status);
};

// The BaseNanopbServerReader serves as the base for the ServerReader and
// ServerReaderWriter classes. It adds a callback templated on the request
// struct type. It is templated on the Request type only.
template <typename Request>
class BaseNanopbServerReader : public GenericNanopbResponder {
 public:
  BaseNanopbServerReader(const internal::ServerCall& call, MethodType type)
      : GenericNanopbResponder(call, type) {}

 protected:
  constexpr BaseNanopbServerReader(MethodType type)
      : GenericNanopbResponder(type) {}

  void set_on_next(Function<void(const Request& request)> on_next) {
    nanopb_on_next_ = std::move(on_next);

    internal::Responder::set_on_next([this](ConstByteSpan payload) {
      Request request_struct;
      DecodeRequest(payload, &request_struct);
      nanopb_on_next_(request_struct);
    });
  }

 private:
  Function<void(const Request&)> nanopb_on_next_;
};

}  // namespace internal

// The NanopbServerReaderWriter is used to send and receive messages in a Nanopb
// bidirectional streaming RPC.
template <typename Request, typename Response>
class NanopbServerReaderWriter
    : private internal::BaseNanopbServerReader<Request> {
 public:
  constexpr NanopbServerReaderWriter()
      : internal::BaseNanopbServerReader<Request>(
            MethodType::kBidirectionalStreaming) {}

  NanopbServerReaderWriter(NanopbServerReaderWriter&&) = default;
  NanopbServerReaderWriter& operator=(NanopbServerReaderWriter&&) = default;

  using internal::GenericNanopbResponder::open;

  using internal::GenericNanopbResponder::channel_id;

  // Writes a response struct. Returns the following Status codes:
  //
  //   OK - the response was successfully sent
  //   FAILED_PRECONDITION - the writer is closed
  //   INTERNAL - pw_rpc was unable to encode the Nanopb protobuf
  //   other errors - the ChannelOutput failed to send the packet; the error
  //       codes are determined by the ChannelOutput implementation
  //
  Status Write(const Response& response) {
    return internal::GenericNanopbResponder::SendClientStream(&response);
  }

  Status Finish(Status status = OkStatus()) {
    return internal::Responder::CloseAndSendResponse(status);
  }

  // Functions for setting RPC event callbacks.
  using internal::BaseNanopbServerReader<Request>::set_on_client_stream_end;
  using internal::BaseNanopbServerReader<Request>::set_on_error;
  using internal::BaseNanopbServerReader<Request>::set_on_next;

 private:
  friend class internal::NanopbMethod;

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerReaderWriter(const internal::ServerCall& call)
      : internal::BaseNanopbServerReader<Request>(
            call, MethodType::kBidirectionalStreaming) {}
};

// The NanopbServerReader is used to receive messages and send a response in a
// Nanopb client streaming RPC.
template <typename Request, typename Response>
class NanopbServerReader : private internal::BaseNanopbServerReader<Request> {
 public:
  // Allow default construction so that users can declare a variable into which
  // to move NanopbServerReaders from RPC calls.
  constexpr NanopbServerReader()
      : internal::BaseNanopbServerReader<Request>(
            MethodType::kClientStreaming) {}

  NanopbServerReader(NanopbServerReader&&) = default;
  NanopbServerReader& operator=(NanopbServerReader&&) = default;

  using internal::GenericNanopbResponder::channel_id;

  // Functions for setting RPC event callbacks.
  using internal::BaseNanopbServerReader<Request>::set_on_client_stream_end;
  using internal::BaseNanopbServerReader<Request>::set_on_error;
  using internal::BaseNanopbServerReader<Request>::set_on_next;

  Status Finish(const Response& response, Status status = OkStatus()) {
    return internal::BaseNanopbServerReader<Request>::SendResponse(&response,
                                                                   status);
  }

 private:
  friend class internal::NanopbMethod;

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerReader(const internal::ServerCall& call)
      : internal::BaseNanopbServerReader<Request>(
            call, MethodType::kClientStreaming) {}
};

// The NanopbServerWriter is used to send responses in a Nanopb server streaming
// RPC.
template <typename Response>
class NanopbServerWriter : private internal::GenericNanopbResponder {
 public:
  // Allow default construction so that users can declare a variable into which
  // to move ServerWriters from RPC calls.
  constexpr NanopbServerWriter()
      : internal::GenericNanopbResponder(MethodType::kServerStreaming) {}

  NanopbServerWriter(NanopbServerWriter&&) = default;
  NanopbServerWriter& operator=(NanopbServerWriter&&) = default;

  using internal::GenericNanopbResponder::open;

  using internal::GenericNanopbResponder::channel_id;

  // Writes a response struct. Returns the following Status codes:
  //
  //   OK - the response was successfully sent
  //   FAILED_PRECONDITION - the writer is closed
  //   INTERNAL - pw_rpc was unable to encode the Nanopb protobuf
  //   other errors - the ChannelOutput failed to send the packet; the error
  //       codes are determined by the ChannelOutput implementation
  //
  Status Write(const Response& response) {
    return internal::GenericNanopbResponder::SendClientStream(&response);
  }

  Status Finish(Status status = OkStatus()) {
    return internal::Responder::CloseAndSendResponse(status);
  }

 private:
  friend class internal::NanopbMethod;

  template <typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerWriter(const internal::ServerCall& call)
      : internal::GenericNanopbResponder(call, MethodType::kServerStreaming) {}
};

// TODO(hepler): "pw::rpc::ServerWriter" should not be specific to Nanopb.
template <typename T>
using ServerWriter = NanopbServerWriter<T>;

}  // namespace pw::rpc
