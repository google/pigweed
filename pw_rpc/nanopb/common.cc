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

#include "pw_rpc/nanopb/internal/common.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "pw_log/log.h"

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

// PB_NO_ERRMSG is used in pb_decode.h and pb_encode.h to enable or disable the
// errmsg member of the istream and ostream structs. If errmsg is available, use
// it to give more detailed log messages.
#ifdef PB_NO_ERRMSG

#define PW_RPC_LOG_NANOPB_FAILURE(msg, stream) PW_LOG_ERROR(msg)

#else

#define PW_RPC_LOG_NANOPB_FAILURE(msg, stream) \
  PW_LOG_ERROR(msg ": %s", stream.errmsg)

#endif  // PB_NO_ERRMSG

StatusWithSize NanopbMethodSerde::Encode(NanopbMessageDescriptor fields,
                                         const void* proto_struct,
                                         ByteSpan buffer) const {
  auto output = pb_ostream_from_buffer(
      reinterpret_cast<pb_byte_t*>(buffer.data()), buffer.size());
  if (!pb_encode(&output, static_cast<Fields>(fields), proto_struct)) {
    PW_RPC_LOG_NANOPB_FAILURE("Nanopb protobuf encode failed", output);
    return StatusWithSize::Internal();
  }

  return StatusWithSize(output.bytes_written);
}

bool NanopbMethodSerde::Decode(NanopbMessageDescriptor fields,
                               ConstByteSpan buffer,
                               void* proto_struct) const {
  auto input = pb_istream_from_buffer(
      reinterpret_cast<const pb_byte_t*>(buffer.data()), buffer.size());
  bool result = pb_decode(&input, static_cast<Fields>(fields), proto_struct);
  if (!result) {
    PW_RPC_LOG_NANOPB_FAILURE("Nanopb protobuf decode failed", input);
  }
  return result;
}

#undef PW_RPC_LOG_NANOPB_FAILURE

}  // namespace pw::rpc::internal
