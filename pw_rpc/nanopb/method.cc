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

#include "pw_rpc/internal/method.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal {
namespace {

// Nanopb 3 uses pb_field_s and Nanopb 4 uses pb_msgdesc_s for fields. The
// Nanopb version macro is difficult to use, so deduce the correct type from the
// pb_decode function.
template <typename DecodeFunction>
struct NanopbTraits;

template <typename FieldsType>
struct NanopbTraits<bool(pb_istream_t*, FieldsType, void*)> {
  using Fields = FieldsType;
};

using Fields = typename NanopbTraits<decltype(pb_decode)>::Fields;

}  // namespace

using std::byte;

StatusWithSize Method::EncodeResponse(const void* proto_struct,
                                      std::span<byte> buffer) const {
  auto output = pb_ostream_from_buffer(
      reinterpret_cast<pb_byte_t*>(buffer.data()), buffer.size());
  if (pb_encode(&output, static_cast<Fields>(response_fields_), proto_struct)) {
    return StatusWithSize(output.bytes_written);
  }
  return StatusWithSize::INTERNAL;
}

bool Method::DecodeResponse(std::span<const byte> response,
                            void* proto_struct) const {
  auto input = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(response.data()), response.size());
  return pb_decode(&input, static_cast<Fields>(response_fields_), proto_struct);
}

void Method::CallUnary(ServerCall& call,
                       const Packet& request,
                       void* request_struct,
                       void* response_struct) const {
  if (!DecodeRequest(call.channel(), request, request_struct)) {
    return;
  }

  const Status status = function_.unary(call, request_struct, response_struct);
  SendResponse(call.channel(), request, response_struct, status);
}

void Method::CallServerStreaming(ServerCall& call,
                                 const Packet& request,
                                 void* request_struct) const {
  if (!DecodeRequest(call.channel(), request, request_struct)) {
    return;
  }

  internal::BaseServerWriter server_writer(call);
  function_.server_streaming(call, request_struct, server_writer);
}

bool Method::DecodeRequest(Channel& channel,
                           const Packet& request,
                           void* proto_struct) const {
  auto input = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(request.payload().data()),
      request.payload().size());
  if (pb_decode(&input, static_cast<Fields>(request_fields_), proto_struct)) {
    return true;
  }

  PW_LOG_WARN("Failed to decode request payload from channel %u",
              unsigned(channel.id()));
  channel.Send(Packet::Error(request, Status::DATA_LOSS));
  return false;
}

void Method::SendResponse(Channel& channel,
                          const Packet& request,
                          const void* response_struct,
                          Status status) const {
  Channel::OutputBuffer response_buffer = channel.AcquireBuffer();
  std::span payload_buffer = response_buffer.payload(request);

  StatusWithSize encoded = EncodeResponse(response_struct, payload_buffer);

  if (encoded.ok()) {
    Packet response = Packet::Response(request);

    response.set_payload(payload_buffer.first(encoded.size()));
    response.set_status(status);
    if (channel.Send(response_buffer, response).ok()) {
      return;
    }
  }

  PW_LOG_WARN("Failed to encode response packet for channel %u",
              unsigned(channel.id()));
  channel.Send(response_buffer, Packet::Error(request, Status::INTERNAL));
}

}  // namespace pw::rpc::internal
