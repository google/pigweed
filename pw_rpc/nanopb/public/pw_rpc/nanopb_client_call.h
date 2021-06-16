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
#pragma once

#include <new>

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_rpc/internal/base_client_call.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_rpc/internal/nanopb_common.h"
#include "pw_status/status.h"

namespace pw::rpc {
namespace internal {

// Non-templated nanopb base class providing protobuf encoding and decoding.
class BaseNanopbClientCall : public BaseClientCall {
 public:
  Status SendRequest(const void* request_struct);

 protected:
  constexpr BaseNanopbClientCall(rpc::Channel* channel,
                                 uint32_t service_id,
                                 uint32_t method_id,
                                 ResponseHandler handler,
                                 NanopbMessageDescriptor request_fields,
                                 NanopbMessageDescriptor response_fields)
      : BaseClientCall(channel, service_id, method_id, handler),
        serde_(request_fields, response_fields) {}

  constexpr BaseNanopbClientCall()
      : BaseClientCall(), serde_(nullptr, nullptr) {}

  BaseNanopbClientCall(const BaseNanopbClientCall&) = delete;
  BaseNanopbClientCall& operator=(const BaseNanopbClientCall&) = delete;

  BaseNanopbClientCall(BaseNanopbClientCall&&) = default;
  BaseNanopbClientCall& operator=(BaseNanopbClientCall&&) = default;

  constexpr const NanopbMethodSerde& serde() const { return serde_; }

 private:
  NanopbMethodSerde serde_;
};

struct ErrorCallbacks {
  ErrorCallbacks() = default;

  ErrorCallbacks(Function<void(Status)> error) : rpc_error(std::move(error)) {}

  void InvokeRpcError(Status status) {
    if (rpc_error != nullptr) {
      rpc_error(status);
    }
  }

  Function<void(Status)> rpc_error;
};

template <typename ResponseType>
struct UnaryCallbacks : public ErrorCallbacks {
  using Response = ResponseType;
  static constexpr MethodType kType = MethodType::kUnary;

  UnaryCallbacks() = default;

  UnaryCallbacks(Function<void(const Response&, Status)> response,
                 Function<void(Status)> error)
      : ErrorCallbacks(std::move(error)), unary_response(std::move(response)) {}

  Function<void(const Response&, Status)> unary_response;
};

template <typename ResponseType>
struct ServerStreamingCallbacks : public ErrorCallbacks {
  using Response = ResponseType;
  static constexpr MethodType kType = MethodType::kServerStreaming;

  ServerStreamingCallbacks() = default;

  ServerStreamingCallbacks(Function<void(const Response&)> response,
                           Function<void(Status)> end,
                           Function<void(Status)> error)
      : ErrorCallbacks(std::move(error)),
        stream_response(std::move(response)),
        stream_end(std::move(end)) {}

  Function<void(const Response&)> stream_response;
  Function<void(Status)> stream_end;
};

}  // namespace internal

template <typename Callbacks>
class NanopbClientCall : public internal::BaseNanopbClientCall {
 public:
  constexpr NanopbClientCall(Channel* channel,
                             uint32_t service_id,
                             uint32_t method_id,
                             Callbacks callbacks,
                             internal::NanopbMessageDescriptor request_fields,
                             internal::NanopbMessageDescriptor response_fields)
      : BaseNanopbClientCall(channel,
                             service_id,
                             method_id,
                             &ResponseHandler,
                             request_fields,
                             response_fields),
        callbacks_(std::move(callbacks)) {}

  constexpr NanopbClientCall() : BaseNanopbClientCall(), callbacks_({}) {}

  NanopbClientCall(const NanopbClientCall&) = delete;
  NanopbClientCall& operator=(const NanopbClientCall&) = delete;

  NanopbClientCall(NanopbClientCall&&) = default;
  NanopbClientCall& operator=(NanopbClientCall&&) = default;

 private:
  using Response = typename Callbacks::Response;

  // Buffer into which the nanopb struct is decoded. Its contents are unknown,
  // so it is aligned to maximum alignment to accommodate any type.
  using ResponseBuffer =
      std::aligned_storage_t<sizeof(Response), alignof(std::max_align_t)>;

  friend class Client;

  static void ResponseHandler(internal::BaseClientCall& call,
                              const internal::Packet& packet) {
    static_cast<NanopbClientCall<Callbacks>&>(call).HandleResponse(packet);
  }

  void HandleResponse(const internal::Packet& packet) {
    if constexpr (Callbacks::kType == internal::MethodType::kUnary) {
      InvokeUnaryCallback(packet);
    }
    if constexpr (Callbacks::kType == internal::MethodType::kServerStreaming) {
      InvokeServerStreamingCallback(packet);
    }
  }

  void InvokeUnaryCallback(const internal::Packet& packet) {
    if (packet.type() == internal::PacketType::SERVER_ERROR) {
      callbacks_.InvokeRpcError(packet.status());
      return;
    }

    ResponseBuffer response_struct{};

    if (callbacks_.unary_response &&
        serde().DecodeResponse(&response_struct, packet.payload())) {
      callbacks_.unary_response(
          *std::launder(reinterpret_cast<Response*>(&response_struct)),
          packet.status());
    } else {
      callbacks_.InvokeRpcError(Status::DataLoss());
    }
  }

  void InvokeServerStreamingCallback(const internal::Packet& packet) {
    if (packet.type() == internal::PacketType::SERVER_ERROR) {
      callbacks_.InvokeRpcError(packet.status());
      return;
    }

    if (packet.type() == internal::PacketType::RESPONSE) {
      // The server sent the last packet; call the stream end callback.
      if (callbacks_.stream_end) {
        callbacks_.stream_end(packet.status());
      }
      return;
    }

    // Handle internal::PacketType::SERVER_STREAM packets.
    ResponseBuffer response_struct{};

    if (callbacks_.stream_response &&
        serde().DecodeResponse(&response_struct, packet.payload())) {
      callbacks_.stream_response(
          *std::launder(reinterpret_cast<Response*>(&response_struct)));
    } else {
      callbacks_.InvokeRpcError(Status::DataLoss());
    }
  }

  Callbacks callbacks_;
};

}  // namespace pw::rpc
