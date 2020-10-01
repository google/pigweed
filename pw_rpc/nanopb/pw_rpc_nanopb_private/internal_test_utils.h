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

#include <span>

#include "pb_decode.h"
#include "pb_encode.h"
#include "pw_rpc/nanopb_client_call.h"

namespace pw::rpc::internal {

// Encodes a protobuf to a local span named by result from a list of nanopb
// struct initializers.
//
//  PW_ENCODE_PB(pw_rpc_TestProto, encoded, .value = 42);
//
#define PW_ENCODE_PB(proto, result, ...) \
  _PW_ENCODE_PB_EXPAND(proto, result, __LINE__, __VA_ARGS__)

#define _PW_ENCODE_PB_EXPAND(proto, result, unique, ...) \
  _PW_ENCODE_PB_IMPL(proto, result, unique, __VA_ARGS__)

#define _PW_ENCODE_PB_IMPL(proto, result, unique, ...)            \
  std::array<pb_byte_t, 2 * sizeof(proto)> _pb_buffer_##unique{}; \
  const std::span result =                                        \
      ::pw::rpc::internal::EncodeProtobuf<proto, proto##_fields>( \
          proto{__VA_ARGS__}, _pb_buffer_##unique)

template <typename T, auto fields>
std::span<const std::byte> EncodeProtobuf(const T& protobuf,
                                          std::span<pb_byte_t> buffer) {
  auto output = pb_ostream_from_buffer(buffer.data(), buffer.size());
  EXPECT_TRUE(pb_encode(&output, fields, &protobuf));
  return std::as_bytes(buffer.first(output.bytes_written));
}

// Decodes a protobuf to a nanopb struct named by result.
#define PW_DECODE_PB(proto, result, buffer)                        \
  proto result;                                                    \
  ::pw::rpc::internal::DecodeProtobuf<proto, proto##_fields>(      \
      std::span(reinterpret_cast<const pb_byte_t*>(buffer.data()), \
                buffer.size()),                                    \
      result);

template <typename T, auto fields>
void DecodeProtobuf(std::span<const pb_byte_t> buffer, T& protobuf) {
  auto input = pb_istream_from_buffer(buffer.data(), buffer.size());
  EXPECT_TRUE(pb_decode(&input, fields, &protobuf));
}

// Client response handler for a unary RPC invocation which captures the
// response it receives.
template <typename Response>
class TestUnaryResponseHandler : public UnaryResponseHandler<Response> {
 public:
  void ReceivedResponse(Status status, const Response& response) override {
    last_status_ = status;
    last_response_ = response;
    ++responses_received_;
  }

  void RpcError(Status status) override { rpc_error_ = status; }

  constexpr Status last_status() const { return last_status_; }
  constexpr const Response& last_response() const& { return last_response_; }
  constexpr size_t responses_received() const { return responses_received_; }
  constexpr Status rpc_error() const { return rpc_error_; }

 private:
  Status last_status_;
  Response last_response_;
  size_t responses_received_ = 0;
  Status rpc_error_;
};

// Client response handler for a unary RPC invocation which stores information
// about the state of the stream.
template <typename Response>
class TestServerStreamingResponseHandler
    : public ServerStreamingResponseHandler<Response> {
 public:
  void ReceivedResponse(const Response& response) override {
    last_response_ = response;
    ++responses_received_;
  }

  void Complete(Status status) override {
    active_ = false;
    status_ = status;
  }

  void RpcError(Status status) override { rpc_error_ = status; }

  constexpr bool active() const { return active_; }
  constexpr Status status() const { return status_; }
  constexpr const Response& last_response() const& { return last_response_; }
  constexpr size_t responses_received() const { return responses_received_; }
  constexpr Status rpc_error() const { return rpc_error_; }

 private:
  Status status_;
  Response last_response_;
  size_t responses_received_ = 0;
  bool active_ = true;
  Status rpc_error_;
};

}  // namespace pw::rpc::internal
