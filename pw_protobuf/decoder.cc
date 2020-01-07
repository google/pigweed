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

#include "pw_protobuf/decoder.h"

#include "pw_varint/varint.h"

namespace pw::protobuf {

Status Decoder::Decode(span<const std::byte> proto) {
  if (handler_ == nullptr || state_ != kReady) {
    return Status::FAILED_PRECONDITION;
  }

  state_ = kDecodeInProgress;
  proto_ = proto;

  // Iterate over each field in the proto, calling the handler with the field
  // key.
  while (state_ == kDecodeInProgress && !proto_.empty()) {
    const std::byte* original_cursor = proto_.data();

    uint64_t key;
    size_t bytes_read = varint::Decode(proto_, &key);
    if (bytes_read == 0) {
      state_ = kDecodeFailed;
      return Status::DATA_LOSS;
    }

    uint32_t field_number = key >> kFieldNumberShift;
    Status status = handler_->ProcessField(this, field_number);
    if (!status.ok()) {
      return status;
    }

    // The callback function can modify the decoder's state; check that
    // everything is still okay.
    if (state_ == kDecodeFailed) {
      break;
    }

    // If the cursor has not moved, the user has not consumed the field in their
    // callback. Skip ahead to the next field.
    if (original_cursor == proto_.data()) {
      SkipField();
    }
  }

  if (state_ != kDecodeInProgress) {
    return Status::DATA_LOSS;
  }

  state_ = kReady;
  return Status::OK;
}

Status Decoder::ReadVarint(uint32_t field_number, uint64_t* out) {
  if (state_ != kDecodeInProgress) {
    return Status::FAILED_PRECONDITION;
  }

  uint64_t key;
  size_t bytes_read = varint::Decode(proto_, &key);
  if (bytes_read == 0) {
    state_ = kDecodeFailed;
    return Status::FAILED_PRECONDITION;
  }

  uint32_t field = key >> kFieldNumberShift;
  WireType wire_type = static_cast<WireType>(key & kWireTypeMask);

  if (field != field_number || wire_type != WireType::kVarint) {
    state_ = kDecodeFailed;
    return Status::FAILED_PRECONDITION;
  }

  // Advance to the varint value.
  proto_ = proto_.subspan(bytes_read);

  bytes_read = varint::Decode(proto_, out);
  if (bytes_read == 0) {
    state_ = kDecodeFailed;
    return Status::DATA_LOSS;
  }

  // Advance to the next field.
  proto_ = proto_.subspan(bytes_read);
  return Status::OK;
}

void Decoder::SkipField() {
  uint64_t key;
  proto_ = proto_.subspan(varint::Decode(proto_, &key));

  WireType wire_type = static_cast<WireType>(key & kWireTypeMask);
  size_t bytes_to_skip = 0;
  uint64_t value = 0;

  switch (wire_type) {
    case WireType::kVarint:
      bytes_to_skip = varint::Decode(proto_, &value);
      break;

    case WireType::kDelimited:
      // Varint at cursor indicates size of the field.
      bytes_to_skip += varint::Decode(proto_, &value);
      bytes_to_skip += value;
      break;

    case WireType::kFixed32:
      bytes_to_skip = sizeof(uint32_t);
      break;

    case WireType::kFixed64:
      bytes_to_skip = sizeof(uint64_t);
      break;
  }

  if (bytes_to_skip == 0) {
    state_ = kDecodeFailed;
  } else {
    proto_ = proto_.subspan(bytes_to_skip);
  }
}

}  // namespace pw::protobuf
