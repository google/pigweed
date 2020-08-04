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

// Serializer/deserializer for nanopb message request and response structs in an
// RPC method.
class NanopbMethodSerde {
 public:
  constexpr NanopbMethodSerde(NanopbMessageDescriptor request_fields,
                              NanopbMessageDescriptor response_fields)
      : request_fields_(request_fields), response_fields_(response_fields) {}

  StatusWithSize EncodeRequest(ByteSpan buffer,
                               const void* proto_struct) const {
    return Encode(request_fields_, buffer, proto_struct);
  }
  StatusWithSize EncodeResponse(ByteSpan buffer,
                                const void* proto_struct) const {
    return Encode(response_fields_, buffer, proto_struct);
  }

  bool DecodeRequest(void* proto_struct, ConstByteSpan buffer) const {
    return Decode(request_fields_, proto_struct, buffer);
  }
  bool DecodeResponse(void* proto_struct, ConstByteSpan buffer) const {
    return Decode(response_fields_, proto_struct, buffer);
  }

 private:
  // Encodes a nanopb protobuf struct to serialized wire format.
  StatusWithSize Encode(NanopbMessageDescriptor fields,
                        ByteSpan buffer,
                        const void* proto_struct) const;

  // Decodes a serialized protobuf to a nanopb struct.
  bool Decode(NanopbMessageDescriptor fields,
              void* proto_struct,
              ConstByteSpan buffer) const;

  NanopbMessageDescriptor request_fields_;
  NanopbMessageDescriptor response_fields_;
};

}  // namespace pw::rpc::internal
