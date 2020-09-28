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

#include <cstring>

#include "pw_varint/varint.h"

namespace pw::protobuf {

Status Decoder::Next() {
  if (!previous_field_consumed_) {
    if (Status status = SkipField(); !status.ok()) {
      return status;
    }
  }
  if (proto_.empty()) {
    return Status::OutOfRange();
  }
  previous_field_consumed_ = false;
  return FieldSize() == 0 ? Status::DataLoss() : Status::Ok();
}

Status Decoder::SkipField() {
  if (proto_.empty()) {
    return Status::OutOfRange();
  }

  size_t bytes_to_skip = FieldSize();
  if (bytes_to_skip == 0) {
    return Status::DataLoss();
  }

  proto_ = proto_.subspan(bytes_to_skip);
  return proto_.empty() ? Status::OutOfRange() : Status::Ok();
}

uint32_t Decoder::FieldNumber() const {
  uint64_t key;
  varint::Decode(proto_, &key);
  return key >> kFieldNumberShift;
}

Status Decoder::ReadUint32(uint32_t* out) {
  uint64_t value = 0;
  Status status = ReadUint64(&value);
  if (!status.ok()) {
    return status;
  }
  if (value > std::numeric_limits<uint32_t>::max()) {
    return Status::OutOfRange();
  }
  *out = value;
  return Status::Ok();
}

Status Decoder::ReadSint32(int32_t* out) {
  int64_t value = 0;
  Status status = ReadSint64(&value);
  if (!status.ok()) {
    return status;
  }
  if (value > std::numeric_limits<int32_t>::max()) {
    return Status::OutOfRange();
  }
  *out = value;
  return Status::Ok();
}

Status Decoder::ReadSint64(int64_t* out) {
  uint64_t value = 0;
  Status status = ReadUint64(&value);
  if (!status.ok()) {
    return status;
  }
  *out = varint::ZigZagDecode(value);
  return Status::Ok();
}

Status Decoder::ReadBool(bool* out) {
  uint64_t value = 0;
  Status status = ReadUint64(&value);
  if (!status.ok()) {
    return status;
  }
  *out = value;
  return Status::Ok();
}

Status Decoder::ReadString(std::string_view* out) {
  std::span<const std::byte> bytes;
  Status status = ReadDelimited(&bytes);
  if (!status.ok()) {
    return status;
  }
  *out = std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
  return Status::Ok();
}

size_t Decoder::FieldSize() const {
  uint64_t key;
  size_t key_size = varint::Decode(proto_, &key);
  if (key_size == 0) {
    return 0;
  }

  std::span<const std::byte> remainder = proto_.subspan(key_size);
  WireType wire_type = static_cast<WireType>(key & kWireTypeMask);
  uint64_t value = 0;
  size_t expected_size = 0;

  switch (wire_type) {
    case WireType::kVarint:
      expected_size = varint::Decode(remainder, &value);
      if (expected_size == 0) {
        return 0;
      }
      break;

    case WireType::kDelimited:
      // Varint at cursor indicates size of the field.
      expected_size = varint::Decode(remainder, &value);
      if (expected_size == 0) {
        return 0;
      }
      expected_size += value;
      break;

    case WireType::kFixed32:
      expected_size = sizeof(uint32_t);
      break;

    case WireType::kFixed64:
      expected_size = sizeof(uint64_t);
      break;
  }

  if (remainder.size() < expected_size) {
    return 0;
  }

  return key_size + expected_size;
}

Status Decoder::ConsumeKey(WireType expected_type) {
  uint64_t key;
  size_t bytes_read = varint::Decode(proto_, &key);
  if (bytes_read == 0) {
    return Status::FailedPrecondition();
  }

  WireType wire_type = static_cast<WireType>(key & kWireTypeMask);
  if (wire_type != expected_type) {
    return Status::FailedPrecondition();
  }

  // Advance past the key.
  proto_ = proto_.subspan(bytes_read);
  return Status::Ok();
}

Status Decoder::ReadVarint(uint64_t* out) {
  if (Status status = ConsumeKey(WireType::kVarint); !status.ok()) {
    return status;
  }

  size_t bytes_read = varint::Decode(proto_, out);
  if (bytes_read == 0) {
    return Status::DataLoss();
  }

  // Advance to the next field.
  proto_ = proto_.subspan(bytes_read);
  previous_field_consumed_ = true;
  return Status::Ok();
}

Status Decoder::ReadFixed(std::byte* out, size_t size) {
  WireType expected_wire_type =
      size == sizeof(uint32_t) ? WireType::kFixed32 : WireType::kFixed64;
  Status status = ConsumeKey(expected_wire_type);
  if (!status.ok()) {
    return status;
  }

  if (proto_.size() < size) {
    return Status::DataLoss();
  }

  std::memcpy(out, proto_.data(), size);
  proto_ = proto_.subspan(size);
  previous_field_consumed_ = true;

  return Status::Ok();
}

Status Decoder::ReadDelimited(std::span<const std::byte>* out) {
  Status status = ConsumeKey(WireType::kDelimited);
  if (!status.ok()) {
    return status;
  }

  uint64_t length;
  size_t bytes_read = varint::Decode(proto_, &length);
  if (bytes_read == 0) {
    return Status::DataLoss();
  }

  proto_ = proto_.subspan(bytes_read);
  if (proto_.size() < length) {
    return Status::DataLoss();
  }

  *out = proto_.first(length);
  proto_ = proto_.subspan(length);
  previous_field_consumed_ = true;

  return Status::Ok();
}

Status CallbackDecoder::Decode(std::span<const std::byte> proto) {
  if (handler_ == nullptr || state_ != kReady) {
    return Status::FailedPrecondition();
  }

  state_ = kDecodeInProgress;
  decoder_.Reset(proto);

  // Iterate the proto, calling the handler with each field number.
  while (state_ == kDecodeInProgress) {
    if (Status status = decoder_.Next(); !status.ok()) {
      if (status == Status::OutOfRange()) {
        // Reached the end of the proto.
        break;
      }

      // Proto data is malformed.
      return status;
    }

    Status status = handler_->ProcessField(*this, decoder_.FieldNumber());
    if (!status.ok()) {
      state_ = status == Status::Cancelled() ? kDecodeCancelled : kDecodeFailed;
      return status;
    }

    // The callback function can modify the decoder's state; check that
    // everything is still okay.
    if (state_ == kDecodeFailed) {
      break;
    }
  }

  if (state_ != kDecodeInProgress) {
    return Status::DataLoss();
  }

  state_ = kReady;
  return Status::Ok();
}

}  // namespace pw::protobuf
