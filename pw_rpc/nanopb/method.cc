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

Status Method::DecodeRequest(span<const byte> buffer,
                             void* proto_struct) const {
  auto input = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(buffer.data()), buffer.size());
  return pb_decode(&input, static_cast<Fields>(request_fields_), proto_struct)
             ? Status::OK
             : Status::INTERNAL;
}

StatusWithSize Method::EncodeResponse(const void* proto_struct,
                                      span<byte> buffer) const {
  auto output = pb_ostream_from_buffer(
      reinterpret_cast<pb_byte_t*>(buffer.data()), buffer.size());
  if (pb_encode(&output, static_cast<Fields>(response_fields_), proto_struct)) {
    return StatusWithSize(output.bytes_written);
  }
  return StatusWithSize::INTERNAL;
}

StatusWithSize Method::CallUnary(ServerCall& call,
                                 span<const byte> request_buffer,
                                 span<byte> response_buffer,
                                 void* request_struct,
                                 void* response_struct) const {
  Status status = DecodeRequest(request_buffer, request_struct);
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }

  status = function_.unary(call.context(), request_struct, response_struct);

  StatusWithSize encoded = EncodeResponse(response_struct, response_buffer);
  if (encoded.ok()) {
    return StatusWithSize(status, encoded.size());
  }
  return encoded;
}

StatusWithSize Method::CallServerStreaming(ServerCall& call,
                                           span<const byte> request_buffer,
                                           void* request_struct) const {
  Status status = DecodeRequest(request_buffer, request_struct);
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }

  internal::BaseServerWriter server_writer(call);
  return StatusWithSize(
      function_.server_streaming(call.context(), request_struct, server_writer),
      0);
}

}  // namespace pw::rpc::internal
