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

#include "pw_bytes/span.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc::internal {

// Use a void* to cover both Nanopb 3's pb_field_s and Nanopb 4's pb_msgdesc_s.
using NanopbMessageDescriptor = const void*;

// Serializer/deserializer for a Nanopb protobuf message.
class NanopbSerde {
 public:
  explicit constexpr NanopbSerde(NanopbMessageDescriptor fields)
      : fields_(fields) {}

  NanopbSerde(const NanopbSerde&) = default;
  NanopbSerde& operator=(const NanopbSerde&) = default;

  // Encodes a Nanopb protobuf struct to the serialized wire format.
  StatusWithSize Encode(const void* proto_struct, ByteSpan buffer) const;

  // Decodes a serialized protobuf to a Nanopb struct.
  bool Decode(ConstByteSpan buffer, void* proto_struct) const;

 private:
  NanopbMessageDescriptor fields_;
};

// Serializer/deserializer for Nanopb message request and response structs in an
// RPC method.
class NanopbMethodSerde {
 public:
  constexpr NanopbMethodSerde(NanopbMessageDescriptor request_fields,
                              NanopbMessageDescriptor response_fields)
      : request_fields_(request_fields), response_fields_(response_fields) {}

  NanopbMethodSerde(const NanopbMethodSerde&) = default;
  NanopbMethodSerde& operator=(const NanopbMethodSerde&) = default;

  StatusWithSize EncodeRequest(const void* proto_struct,
                               ByteSpan buffer) const {
    return request_fields_.Encode(proto_struct, buffer);
  }
  StatusWithSize EncodeResponse(const void* proto_struct,
                                ByteSpan buffer) const {
    return response_fields_.Encode(proto_struct, buffer);
  }

  bool DecodeRequest(ConstByteSpan buffer, void* proto_struct) const {
    return request_fields_.Decode(buffer, proto_struct);
  }
  bool DecodeResponse(ConstByteSpan buffer, void* proto_struct) const {
    return response_fields_.Decode(buffer, proto_struct);
  }

  const NanopbSerde& request() const { return request_fields_; }
  const NanopbSerde& response() const { return response_fields_; }

 private:
  NanopbSerde request_fields_;
  NanopbSerde response_fields_;
};

}  // namespace pw::rpc::internal
