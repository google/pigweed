// Copyright 2021 The Pigweed Authors
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

#include "pw_protobuf/encoder.h"

#include <cstddef>
#include <cstring>
#include <span>

#include "pw_assert/check.h"
#include "pw_bytes/span.h"
#include "pw_protobuf/wire_format.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/memory_stream.h"
#include "pw_stream/stream.h"
#include "pw_varint/varint.h"

namespace pw::protobuf {

StreamEncoder StreamEncoder::GetNestedEncoder(uint32_t field_number) {
  PW_CHECK(!nested_encoder_open());
  nested_field_number_ = field_number;

  // Pass the unused space of the scratch buffer to the nested encoder to use
  // as their scratch buffer.
  size_t key_size =
      varint::EncodedSize(MakeKey(field_number, WireType::kDelimited));
  size_t reserved_size = key_size + config::kMaxVarintSize;
  size_t max_size = std::min(memory_writer_.ConservativeWriteLimit(),
                             writer_.ConservativeWriteLimit());
  // Account for reserved bytes.
  max_size = max_size > reserved_size ? max_size - reserved_size : 0;
  // Cap based on max varint size.
  max_size = std::min(varint::MaxValueInBytes(config::kMaxVarintSize),
                      static_cast<uint64_t>(max_size));

  ByteSpan nested_buffer;
  if (max_size > 0) {
    nested_buffer = ByteSpan(
        memory_writer_.data() + reserved_size + memory_writer_.bytes_written(),
        max_size);
  } else {
    nested_buffer = ByteSpan();
  }
  return StreamEncoder(*this, nested_buffer);
}

Status StreamEncoder::Finalize() {
  // If an encoder has no parent, finalize is a no-op.
  if (parent_ == nullptr) {
    return OkStatus();
  }

  PW_CHECK(!nested_encoder_open(),
           "Tried to finalize a proto encoder when it has an active submessage "
           "encoder that hasn't been finalized!");
  // MemoryWriters with their parent set to themselves are externally
  // created by users. It's not valid or necessary to call
  // FinalizeNestedMessage() on themselves.
  if (parent_ == this) {
    return OkStatus();
  }

  return parent_->FinalizeNestedMessage(*this);
}

Status StreamEncoder::FinalizeNestedMessage(StreamEncoder& nested) {
  PW_DCHECK_PTR_EQ(
      nested.parent_,
      this,
      "FinalizeNestedMessage() called on the wrong Encoder parent");

  // Make the nested encoder look like it has an open child to block writes for
  // the remainder of the object's life.
  nested.nested_field_number_ = kFirstReservedNumber;
  nested.parent_ = nullptr;
  // Temporarily cache the field number of the child so we can re-enable
  // writing to this encoder.
  uint32_t temp_field_number = nested_field_number_;
  nested_field_number_ = 0;

  // TODO(amontanez): If a submessage fails, we could optionally discard
  // it and continue happily. For now, we'll always invalidate the entire
  // encoder if a single submessage fails.
  status_.Update(nested.status_);
  PW_TRY(status_);

  if (varint::EncodedSize(nested.memory_writer_.bytes_written()) >
      config::kMaxVarintSize) {
    status_ = Status::OutOfRange();
    return status_;
  }

  status_ = WriteLengthDelimitedField(temp_field_number,
                                      nested.memory_writer_.WrittenData());
  return status_;
}

Status StreamEncoder::WriteVarintField(uint32_t field_number, uint64_t value) {
  PW_TRY(UpdateStatusForWrite(
      field_number, WireType::kVarint, varint::EncodedSize(value)));

  WriteVarint(MakeKey(field_number, WireType::kVarint))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  return WriteVarint(value);
}

Status StreamEncoder::WriteLengthDelimitedField(uint32_t field_number,
                                                ConstByteSpan data) {
  PW_TRY(UpdateStatusForWrite(field_number, WireType::kDelimited, data.size()));
  WriteVarint(MakeKey(field_number, WireType::kDelimited))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  WriteVarint(data.size_bytes())
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  if (Status status = writer_.Write(data); !status.ok()) {
    status_ = status;
  }
  return status_;
}

Status StreamEncoder::WriteFixed(uint32_t field_number, ConstByteSpan data) {
  WireType type =
      data.size() == sizeof(uint32_t) ? WireType::kFixed32 : WireType::kFixed64;

  PW_TRY(UpdateStatusForWrite(field_number, type, data.size()));

  WriteVarint(MakeKey(field_number, type))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  if (Status status = writer_.Write(data); !status.ok()) {
    status_ = status;
  }
  return status_;
}

// Encodes a base-128 varint to the buffer. This function assumes the caller
// has already checked UpdateStatusForWrite() to ensure the writer's
// conservative write limit indicates the Writer has sufficient buffer space.
Status StreamEncoder::WriteVarint(uint64_t value) {
  if (!status_.ok()) {
    return status_;
  }

  std::array<std::byte, varint::kMaxVarint64SizeBytes> varint_encode_buffer;
  size_t varint_size = pw::varint::EncodeLittleEndianBase128(
      value, std::span(varint_encode_buffer));

  if (Status status =
          writer_.Write(std::span(varint_encode_buffer).first(varint_size));
      !status.ok()) {
    status_ = status;
    return status_;
  }

  return OkStatus();
}

Status StreamEncoder::WritePackedFixed(uint32_t field_number,
                                       std::span<const std::byte> values,
                                       size_t elem_size) {
  if (values.empty()) {
    return status_;
  }

  PW_CHECK_NOTNULL(values.data());
  PW_DCHECK(elem_size == sizeof(uint32_t) || elem_size == sizeof(uint64_t));

  PW_TRY(UpdateStatusForWrite(
      field_number, WireType::kDelimited, values.size_bytes()));
  WriteVarint(MakeKey(field_number, WireType::kDelimited))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  WriteVarint(values.size_bytes())
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

  for (auto val_start = values.begin(); val_start != values.end();
       val_start += elem_size) {
    // Allocates 8 bytes so both 4-byte and 8-byte types can be encoded as
    // little-endian for serialization.
    std::array<std::byte, sizeof(uint64_t)> data;
    if (std::endian::native == std::endian::little) {
      std::copy(val_start, val_start + elem_size, std::begin(data));
    } else {
      std::reverse_copy(val_start, val_start + elem_size, std::begin(data));
    }
    status_.Update(writer_.Write(std::span(data).first(elem_size)));
    PW_TRY(status_);
  }
  return status_;
}

Status StreamEncoder::UpdateStatusForWrite(uint32_t field_number,
                                           WireType type,
                                           size_t data_size) {
  PW_CHECK(!nested_encoder_open());
  if (!status_.ok()) {
    return status_;
  }
  if (!ValidFieldNumber(field_number)) {
    status_ = Status::InvalidArgument();
    return status_;
  }

  size_t size = varint::EncodedSize(MakeKey(field_number, type));
  if (type == WireType::kDelimited) {
    size += varint::EncodedSize(data_size);
  }
  size += data_size;

  if (size > writer_.ConservativeWriteLimit()) {
    status_ = Status::ResourceExhausted();
  }
  return status_;
}

}  // namespace pw::protobuf
