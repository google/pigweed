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

#include "pw_rpc/internal/nanopb_common.h"

#include "pb_decode.h"
#include "pb_encode.h"

namespace pw::rpc::internal {

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

StatusWithSize NanopbMethodSerde::Encode(NanopbMessageDescriptor fields,
                                         ByteSpan buffer,
                                         const void* proto_struct) const {
  auto output = pb_ostream_from_buffer(
      reinterpret_cast<pb_byte_t*>(buffer.data()), buffer.size());
  if (!pb_encode(&output, static_cast<Fields>(fields), proto_struct)) {
    return StatusWithSize::Internal();
  }

  return StatusWithSize::Ok(output.bytes_written);
}

bool NanopbMethodSerde::Decode(NanopbMessageDescriptor fields,
                               void* proto_struct,
                               ConstByteSpan buffer) const {
  auto input = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(buffer.data()), buffer.size());
  return pb_decode(&input, static_cast<Fields>(fields), proto_struct);
}

}  // namespace pw::rpc::internal
