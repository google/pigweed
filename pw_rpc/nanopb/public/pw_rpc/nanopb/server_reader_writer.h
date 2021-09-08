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
#include "pw_rpc/internal/call.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/open_call.h"
#include "pw_rpc/nanopb/internal/common.h"
#include "pw_rpc/server.h"

namespace pw::rpc {
namespace internal {

// Forward declarations for internal classes needed in friend statements.
class NanopbMethod;

namespace test {

template <typename, typename, uint32_t>
class InvocationContext;

}  // namespace test

// Non-templated base so the methods are instantiated only once.
class GenericNanopbResponder : public internal::ServerCall {
 public:
  constexpr GenericNanopbResponder(MethodType type)
      : internal::ServerCall(type), serde_(nullptr) {}

  GenericNanopbResponder(const CallContext& context, MethodType type);

  GenericNanopbResponder(GenericNanopbResponder&&) = default;
  GenericNanopbResponder& operator=(GenericNanopbResponder&&) = default;

  Status SendResponse(const void* response, Status status) {
    return SendClientStreamOrResponse(response, &status);
  }

 protected:
  Status SendClientStream(const void* response) {
    return SendClientStreamOrResponse(response, nullptr);
  }

  void DecodeRequest(ConstByteSpan payload, void* request_struct) const {
    serde_->DecodeRequest(payload, request_struct);
  }

 private:
  Status SendClientStreamOrResponse(const void* response, const Status* status);

  const NanopbMethodSerde* serde_;
};

// The BaseNanopbServerReader serves as the base for the ServerReader and
// ServerReaderWriter classes. It adds a callback templated on the request
// struct type. It is templated on the Request type only.
template <typename Request>
class BaseNanopbServerReader : public GenericNanopbResponder {
 public:
  BaseNanopbServerReader(const internal::CallContext& context, MethodType type)
      : GenericNanopbResponder(context, type) {}

 protected:
  constexpr BaseNanopbServerReader(MethodType type)
      : GenericNanopbResponder(type) {}

  void set_on_next(Function<void(const Request& request)> on_next) {
    nanopb_on_next_ = std::move(on_next);

    internal::Call::set_on_next([this](ConstByteSpan payload) {
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
  // Creates a NanopbServerReaderWriter that is ready to send responses for a
  // particular RPC. This can be used for testing or to send responses to an RPC
  // that has not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static NanopbServerReaderWriter Open(Server& server,
                                                     uint32_t channel_id,
                                                     ServiceImpl& service) {
    using Info = internal::MethodInfo<kMethod>;
    static_assert(std::is_same_v<Request, typename Info::Request>,
                  "The request type of a NanopbServerReaderWriter must match "
                  "the method.");
    static_assert(std::is_same_v<Response, typename Info::Response>,
                  "The response type of a NanopbServerReaderWriter must match "
                  "the method.");
    return {internal::OpenContext<kMethod, MethodType::kBidirectionalStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetNanopbMethod<ServiceImpl,
                                                Info::kMethodId>())};
  }

  constexpr NanopbServerReaderWriter()
      : internal::BaseNanopbServerReader<Request>(
            MethodType::kBidirectionalStreaming) {}

  NanopbServerReaderWriter(NanopbServerReaderWriter&&) = default;
  NanopbServerReaderWriter& operator=(NanopbServerReaderWriter&&) = default;

  using internal::GenericNanopbResponder::active;

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
    return internal::Call::CloseAndSendResponse(status);
  }

  // Functions for setting RPC event callbacks.
  using internal::BaseNanopbServerReader<Request>::set_on_client_stream_end;
  using internal::BaseNanopbServerReader<Request>::set_on_error;
  using internal::BaseNanopbServerReader<Request>::set_on_next;

 private:
  friend class internal::NanopbMethod;

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerReaderWriter(const internal::CallContext& context)
      : internal::BaseNanopbServerReader<Request>(
            context, MethodType::kBidirectionalStreaming) {}
};

// The NanopbServerReader is used to receive messages and send a response in a
// Nanopb client streaming RPC.
template <typename Request, typename Response>
class NanopbServerReader : private internal::BaseNanopbServerReader<Request> {
 public:
  // Creates a NanopbServerReader that is ready to send a response to a
  // particular RPC. This can be used for testing or to finish an RPC that has
  // not been started by the client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static NanopbServerReader Open(Server& server,
                                               uint32_t channel_id,
                                               ServiceImpl& service) {
    using Info = internal::MethodInfo<kMethod>;
    static_assert(
        std::is_same_v<Request, typename Info::Request>,
        "The request type of a NanopbServerReader must match the method.");
    static_assert(
        std::is_same_v<Response, typename Info::Response>,
        "The response type of a NanopbServerReader must match the method.");
    return {internal::OpenContext<kMethod, MethodType::kClientStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetNanopbMethod<ServiceImpl,
                                                Info::kMethodId>())};
  }

  // Allow default construction so that users can declare a variable into which
  // to move NanopbServerReaders from RPC calls.
  constexpr NanopbServerReader()
      : internal::BaseNanopbServerReader<Request>(
            MethodType::kClientStreaming) {}

  NanopbServerReader(NanopbServerReader&&) = default;
  NanopbServerReader& operator=(NanopbServerReader&&) = default;

  using internal::GenericNanopbResponder::active;
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

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerReader(const internal::CallContext& context)
      : internal::BaseNanopbServerReader<Request>(
            context, MethodType::kClientStreaming) {}
};

// The NanopbServerWriter is used to send responses in a Nanopb server streaming
// RPC.
template <typename Response>
class NanopbServerWriter : private internal::GenericNanopbResponder {
 public:
  // Creates a NanopbServerWriter that is ready to send responses for a
  // particular RPC. This can be used for testing or to send responses to an RPC
  // that has not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static NanopbServerWriter Open(Server& server,
                                               uint32_t channel_id,
                                               ServiceImpl& service) {
    using Info = internal::MethodInfo<kMethod>;
    static_assert(
        std::is_same_v<Response, typename Info::Response>,
        "The response type of a NanopbServerWriter must match the method.");
    return {internal::OpenContext<kMethod, MethodType::kServerStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetNanopbMethod<ServiceImpl,
                                                Info::kMethodId>())};
  }

  // Allow default construction so that users can declare a variable into which
  // to move ServerWriters from RPC calls.
  constexpr NanopbServerWriter()
      : internal::GenericNanopbResponder(MethodType::kServerStreaming) {}

  NanopbServerWriter(NanopbServerWriter&&) = default;
  NanopbServerWriter& operator=(NanopbServerWriter&&) = default;

  using internal::GenericNanopbResponder::active;
  using internal::GenericNanopbResponder::channel_id;
  using internal::GenericNanopbResponder::open;

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
    return internal::Call::CloseAndSendResponse(status);
  }

 private:
  friend class internal::NanopbMethod;

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerWriter(const internal::CallContext& context)
      : internal::GenericNanopbResponder(context,
                                         MethodType::kServerStreaming) {}
};

template <typename Response>
class NanopbServerResponder : private internal::GenericNanopbResponder {
 public:
  // Creates a NanopbServerResponder that is ready to send a response for a
  // particular RPC. This can be used for testing or to send responses to an RPC
  // that has not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static NanopbServerResponder Open(Server& server,
                                                  uint32_t channel_id,
                                                  ServiceImpl& service) {
    using Info = internal::MethodInfo<kMethod>;
    static_assert(
        std::is_same_v<Response, typename Info::Response>,
        "The response type of a NanopbServerResponder must match the method.");
    return {internal::OpenContext<kMethod, MethodType::kUnary>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetNanopbMethod<ServiceImpl,
                                                Info::kMethodId>())};
  }

  // Allow default construction so that users can declare a variable into which
  // to move ServerWriters from RPC calls.
  constexpr NanopbServerResponder()
      : internal::GenericNanopbResponder(MethodType::kUnary) {}

  NanopbServerResponder(NanopbServerResponder&&) = default;
  NanopbServerResponder& operator=(NanopbServerResponder&&) = default;

  using internal::GenericNanopbResponder::active;
  using internal::GenericNanopbResponder::channel_id;

  // Sends the response. Returns the following Status codes:
  //
  //   OK - the response was successfully sent
  //   FAILED_PRECONDITION - the writer is closed
  //   INTERNAL - pw_rpc was unable to encode the Nanopb protobuf
  //   other errors - the ChannelOutput failed to send the packet; the error
  //       codes are determined by the ChannelOutput implementation
  //
  Status Finish(const Response& response, Status status = OkStatus()) {
    return internal::GenericNanopbResponder::SendResponse(&response, status);
  }

 private:
  friend class internal::NanopbMethod;

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  NanopbServerResponder(const internal::CallContext& context)
      : internal::GenericNanopbResponder(context, MethodType::kUnary) {}
};
// TODO(hepler): "pw::rpc::ServerWriter" should not be specific to Nanopb.
template <typename T>
using ServerWriter = NanopbServerWriter<T>;

}  // namespace pw::rpc
