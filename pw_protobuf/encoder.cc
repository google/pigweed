// Copyright 2019 The Pigweed Authors
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

namespace pw::protobuf {

Status Encoder::WriteUint64(uint32_t field_number, uint64_t value) {
  std::byte* original_cursor = cursor_;
  WriteFieldKey(field_number, WireType::kVarint);
  Status status = WriteVarint(value);
  IncreaseParentSize(cursor_ - original_cursor);
  return status;
}

// Encodes a base-128 varint to the buffer.
Status Encoder::WriteVarint(uint64_t value) {
  if (!encode_status_.ok()) {
    return encode_status_;
  }

  std::span varint_buf = buffer_.last(RemainingSize());
  if (varint_buf.empty()) {
    encode_status_ = Status::ResourceExhausted();
    return encode_status_;
  }

  size_t written = pw::varint::EncodeLittleEndianBase128(value, varint_buf);
  if (written == 0) {
    encode_status_ = Status::ResourceExhausted();
    return encode_status_;
  }

  cursor_ += written;
  return Status::Ok();
}

Status Encoder::WriteRawBytes(const std::byte* ptr, size_t size) {
  if (!encode_status_.ok()) {
    return encode_status_;
  }

  if (size > RemainingSize()) {
    encode_status_ = Status::ResourceExhausted();
    return encode_status_;
  }

  // Memmove the value into place as it's possible that it shares the encode
  // buffer on a memory-constrained system.
  std::memmove(cursor_, ptr, size);

  cursor_ += size;
  return Status::Ok();
}

Status Encoder::Push(uint32_t field_number) {
  if (!encode_status_.ok()) {
    return encode_status_;
  }

  if (blob_count_ == blob_locations_.size() || depth_ == blob_stack_.size()) {
    encode_status_ = Status::ResourceExhausted();
    return encode_status_;
  }

  // Write the key for the nested field.
  std::byte* original_cursor = cursor_;
  if (Status status = WriteFieldKey(field_number, WireType::kDelimited);
      !status.ok()) {
    encode_status_ = status;
    return status;
  }

  if (sizeof(SizeType) > RemainingSize()) {
    // Rollback if there isn't enough space.
    cursor_ = original_cursor;
    encode_status_ = Status::ResourceExhausted();
    return encode_status_;
  }

  // Update parent size with the written key.
  IncreaseParentSize(cursor_ - original_cursor);

  union {
    std::byte* cursor;
    SizeType* size_cursor;
  };

  // Create a size entry for the new blob and append it to both the nesting
  // stack and location list.
  cursor = cursor_;
  *size_cursor = 0;
  blob_locations_[blob_count_++] = size_cursor;
  blob_stack_[depth_++] = size_cursor;

  cursor_ += sizeof(*size_cursor);
  return Status::Ok();
}

Status Encoder::Pop() {
  if (!encode_status_.ok()) {
    return encode_status_;
  }

  if (depth_ == 0) {
    encode_status_ = Status::FailedPrecondition();
    return encode_status_;
  }

  // Update the parent's size with how much total space the child will take
  // after its size field is varint encoded.
  SizeType child_size = *blob_stack_[--depth_];
  IncreaseParentSize(child_size + VarintSizeBytes(child_size));

  // Encode the child
  if (Status status = EncodeFrom(blob_count_ - 1).status(); !status.ok()) {
    encode_status_ = status;
    return encode_status_;
  }
  blob_count_--;

  return Status::Ok();
}

Result<ConstByteSpan> Encoder::Encode() { return EncodeFrom(0); }

Result<ConstByteSpan> Encoder::EncodeFrom(size_t blob) {
  if (!encode_status_.ok()) {
    return encode_status_;
  }

  if (blob >= blob_count_) {
    // If there are no nested blobs, the buffer already contains a valid proto.
    return Result<ConstByteSpan>(buffer_.first(EncodedSize()));
  }

  union {
    std::byte* read_cursor;
    SizeType* size_cursor;
  };

  // Starting from the first blob, encode each size field as a varint and
  // shift all subsequent data downwards.
  size_cursor = blob_locations_[blob];
  std::byte* write_cursor = read_cursor;

  while (read_cursor < cursor_) {
    SizeType nested_size = *size_cursor;

    std::span<std::byte> varint_buf(write_cursor, sizeof(*size_cursor));
    size_t varint_size =
        pw::varint::EncodeLittleEndianBase128(nested_size, varint_buf);

    // Place the write cursor after the encoded varint and the read cursor at
    // the location of the next proto field.
    write_cursor += varint_size;
    read_cursor += varint_buf.size();

    size_t to_copy;

    if (blob == blob_count_ - 1) {
      to_copy = cursor_ - read_cursor;
    } else {
      std::byte* end = reinterpret_cast<std::byte*>(blob_locations_[blob + 1]);
      to_copy = end - read_cursor;
    }

    std::memmove(write_cursor, read_cursor, to_copy);
    write_cursor += to_copy;
    read_cursor += to_copy;

    ++blob;
  }

  // Point the cursor to the end of the encoded proto.
  cursor_ = write_cursor;
  return Result<ConstByteSpan>(buffer_.first(EncodedSize()));
}

}  // namespace pw::protobuf
