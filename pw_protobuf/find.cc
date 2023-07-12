// Copyright 2023 The Pigweed Authors
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

#include "pw_protobuf/find.h"

#include "pw_protobuf/wire_format.h"

namespace pw::protobuf::internal {

Status AdvanceToField(Decoder& decoder, uint32_t field_number) {
  if (!ValidFieldNumber(field_number)) {
    return Status::InvalidArgument();
  }

  Status status;

  while ((status = decoder.Next()).ok()) {
    if (decoder.FieldNumber() == field_number) {
      return OkStatus();
    }
  }

  // As this is a backend for the Find() APIs, remap OUT_OF_RANGE to NOT_FOUND.
  return status.IsOutOfRange() ? Status::NotFound() : status;
}

Status AdvanceToField(StreamDecoder& decoder, uint32_t field_number) {
  if (!ValidFieldNumber(field_number)) {
    return Status::InvalidArgument();
  }

  Status status;

  while ((status = decoder.Next()).ok()) {
    PW_TRY_ASSIGN(uint32_t field, decoder.FieldNumber());
    if (field == field_number) {
      return OkStatus();
    }
  }

  // As this is a backend for the Find() APIs, remap OUT_OF_RANGE to NOT_FOUND.
  return status.IsOutOfRange() ? Status::NotFound() : status;
}

}  // namespace pw::protobuf::internal
