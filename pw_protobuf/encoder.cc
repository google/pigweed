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
#include "pw_protobuf/serialized_size.h"
#include "pw_protobuf/wire_format.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/memory_stream.h"
#include "pw_stream/stream.h"
#include "pw_varint/varint.h"

namespace pw::protobuf {

StreamEncoder StreamEncoder::GetNestedEncoder(uint32_t field_number) {
  PW_CHECK(!nested_encoder_open());
  PW_CHECK(ValidFieldNumber(field_number));

  nested_field_number_ = field_number;

  // Pass the unused space of the scratch buffer to the nested encoder to use
  // as their scratch buffer.
  size_t key_size =
      varint::EncodedSize(FieldKey(field_number, WireType::kDelimited));
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

StreamEncoder::~StreamEncoder() {
  // If this was an invalidated StreamEncoder which cannot be used, permit the
  // object to be cleanly destructed by doing nothing.
  if (nested_field_number_ == kFirstReservedNumber) {
    return;
  }

  PW_CHECK(
      !nested_encoder_open(),
      "Tried to destruct a proto encoder with an active submessage encoder");

  if (parent_ != nullptr) {
    parent_->CloseNestedMessage(*this);
  }
}

void StreamEncoder::CloseNestedMessage(StreamEncoder& nested) {
  PW_DCHECK_PTR_EQ(nested.parent_,
                   this,
                   "CloseNestedMessage() called on the wrong Encoder parent");

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
  if (!status_.ok()) {
    return;
  }

  if (varint::EncodedSize(nested.memory_writer_.bytes_written()) >
      config::kMaxVarintSize) {
    status_ = Status::OutOfRange();
    return;
  }

  status_ = WriteLengthDelimitedField(temp_field_number,
                                      nested.memory_writer_.WrittenData());
}

Status StreamEncoder::WriteVarintField(uint32_t field_number, uint64_t value) {
  PW_TRY(UpdateStatusForWrite(
      field_number, WireType::kVarint, varint::EncodedSize(value)));

  WriteVarint(FieldKey(field_number, WireType::kVarint))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  return WriteVarint(value);
}

Status StreamEncoder::WriteLengthDelimitedField(uint32_t field_number,
                                                ConstByteSpan data) {
  PW_TRY(UpdateStatusForWrite(field_number, WireType::kDelimited, data.size()));
  status_.Update(WriteLengthDelimitedKeyAndLengthPrefix(
      field_number, data.size(), writer_));
  PW_TRY(status_);
  if (Status status = writer_.Write(data); !status.ok()) {
    status_ = status;
  }
  return status_;
}

Status StreamEncoder::WriteLengthDelimitedFieldFromStream(
    uint32_t field_number,
    stream::Reader& bytes_reader,
    size_t num_bytes,
    ByteSpan stream_pipe_buffer) {
  PW_CHECK_UINT_GT(
      stream_pipe_buffer.size(), 0, "Transfer buffer cannot be 0 size");
  PW_TRY(UpdateStatusForWrite(field_number, WireType::kDelimited, num_bytes));
  status_.Update(
      WriteLengthDelimitedKeyAndLengthPrefix(field_number, num_bytes, writer_));
  PW_TRY(status_);

  // Stream data from `bytes_reader` to `writer_`.
  // TODO(pwbug/468): move the following logic to pw_stream/copy.h at a later
  // time.
  for (size_t bytes_written = 0; bytes_written < num_bytes;) {
    const size_t chunk_size_bytes =
        std::min(num_bytes - bytes_written, stream_pipe_buffer.size_bytes());
    const Result<ByteSpan> read_result =
        bytes_reader.Read(stream_pipe_buffer.data(), chunk_size_bytes);
    status_.Update(read_result.status());
    PW_TRY(status_);

    status_.Update(writer_.Write(read_result.value()));
    PW_TRY(status_);

    bytes_written += read_result.value().size();
  }

  return OkStatus();
}

Status StreamEncoder::WriteFixed(uint32_t field_number, ConstByteSpan data) {
  WireType type =
      data.size() == sizeof(uint32_t) ? WireType::kFixed32 : WireType::kFixed64;

  PW_TRY(UpdateStatusForWrite(field_number, type, data.size()));

  WriteVarint(FieldKey(field_number, type))
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  if (Status status = writer_.Write(data); !status.ok()) {
    status_ = status;
  }
  return status_;
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
  WriteVarint(FieldKey(field_number, WireType::kDelimited))
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
  PW_TRY(status_);

  if (!ValidFieldNumber(field_number)) {
    return status_ = Status::InvalidArgument();
  }

  const Result<size_t> field_size = SizeOfField(field_number, type, data_size);
  status_.Update(field_size.status());
  PW_TRY(status_);

  if (field_size.value() > writer_.ConservativeWriteLimit()) {
    status_ = Status::ResourceExhausted();
  }

  return status_;
}

}  // namespace pw::protobuf
