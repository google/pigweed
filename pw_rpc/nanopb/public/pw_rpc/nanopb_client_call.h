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
#include "pw_rpc/internal/base_client_call.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_rpc/internal/nanopb_common.h"
#include "pw_status/status.h"

namespace pw::rpc {

// Response handler callback for unary RPC methods.
template <typename Response>
class UnaryResponseHandler {
 public:
  virtual ~UnaryResponseHandler() = default;

  // Called when the response is received from the server with the method's
  // status and the deserialized response struct.
  virtual void ReceivedResponse(Status status, const Response& response) = 0;

  // Called when an error occurs internally in the RPC client or server.
  virtual void RpcError(Status) {}
};

// Response handler callbacks for server streaming RPC methods.
template <typename Response>
class ServerStreamingResponseHandler {
 public:
  virtual ~ServerStreamingResponseHandler() = default;

  // Called on every response received from the server with the deserialized
  // response struct.
  virtual void ReceivedResponse(const Response& response) = 0;

  // Called when the server ends the stream with the overall RPC status.
  virtual void Complete(Status status) = 0;

  // Called when an error occurs internally in the RPC client or server.
  virtual void RpcError(Status) {}
};

namespace internal {

// Non-templated nanopb base class providing protobuf encoding and decoding.
class BaseNanopbClientCall : public BaseClientCall {
 public:
  Status SendRequest(const void* request_struct);

 protected:
  constexpr BaseNanopbClientCall(
      rpc::Channel* channel,
      uint32_t service_id,
      uint32_t method_id,
      ResponseHandler handler,
      internal::NanopbMessageDescriptor request_fields,
      internal::NanopbMessageDescriptor response_fields)
      : BaseClientCall(channel, service_id, method_id, handler),
        serde_(request_fields, response_fields) {}

  constexpr const internal::NanopbMethodSerde& serde() const { return serde_; }

 private:
  internal::NanopbMethodSerde serde_;
};

template <typename Callback>
struct CallbackTraits {};

template <typename ResponseType>
struct CallbackTraits<UnaryResponseHandler<ResponseType>> {
  using Response = ResponseType;

  static constexpr MethodType kType = MethodType::kUnary;
};

template <typename ResponseType>
struct CallbackTraits<ServerStreamingResponseHandler<ResponseType>> {
  using Response = ResponseType;

  static constexpr MethodType kType = MethodType::kServerStreaming;
};

}  // namespace internal

template <typename Callback>
class NanopbClientCall : public internal::BaseNanopbClientCall {
 public:
  constexpr NanopbClientCall(Channel* channel,
                             uint32_t service_id,
                             uint32_t method_id,
                             Callback& callback,
                             internal::NanopbMessageDescriptor request_fields,
                             internal::NanopbMessageDescriptor response_fields)
      : BaseNanopbClientCall(channel,
                             service_id,
                             method_id,
                             &ResponseHandler,
                             request_fields,
                             response_fields),
        callback_(callback) {}

 private:
  using Traits = internal::CallbackTraits<Callback>;
  using Response = typename Traits::Response;

  // Buffer into which the nanopb struct is decoded. Its contents are unknown,
  // so it is aligned to maximum alignment to accommodate any type.
  using ResponseBuffer =
      std::aligned_storage_t<sizeof(Response), alignof(std::max_align_t)>;

  friend class Client;

  static void ResponseHandler(internal::BaseClientCall& call,
                              const internal::Packet& packet) {
    static_cast<NanopbClientCall<Callback>&>(call).HandleResponse(packet);
  }

  void HandleResponse(const internal::Packet& packet) {
    if constexpr (Traits::kType == internal::MethodType::kUnary) {
      InvokeUnaryCallback(packet);
    }
    if constexpr (Traits::kType == internal::MethodType::kServerStreaming) {
      InvokeServerStreamingCallback(packet);
    }
  }

  void InvokeUnaryCallback(const internal::Packet& packet) {
    if (packet.type() == internal::PacketType::SERVER_ERROR) {
      callback_.RpcError(packet.status());
      return;
    }

    ResponseBuffer response_struct{};

    if (serde().DecodeResponse(&response_struct, packet.payload())) {
      callback_.ReceivedResponse(
          packet.status(),
          *std::launder(reinterpret_cast<Response*>(&response_struct)));
    } else {
      callback_.RpcError(Status::DataLoss());
    }

    Unregister();
  }

  void InvokeServerStreamingCallback(const internal::Packet& packet) {
    if (packet.type() == internal::PacketType::SERVER_ERROR) {
      callback_.RpcError(packet.status());
      return;
    }

    if (packet.type() == internal::PacketType::SERVER_STREAM_END) {
      callback_.Complete(packet.status());
      return;
    }

    ResponseBuffer response_struct{};

    if (serde().DecodeResponse(&response_struct, packet.payload())) {
      callback_.ReceivedResponse(
          *std::launder(reinterpret_cast<Response*>(&response_struct)));
    } else {
      callback_.RpcError(Status::DataLoss());
    }
  }

  Callback& callback_;
};

}  // namespace pw::rpc
