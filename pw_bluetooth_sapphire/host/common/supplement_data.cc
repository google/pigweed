// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/common/supplement_data.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

#pragma clang diagnostic ignored "-Wswitch-enum"

namespace bt {

bool ParseUuids(const BufferView& data,
                UUIDElemSize uuid_size,
                UuidFunction func) {
  BT_ASSERT(func);

  if (data.size() % uuid_size) {
    return false;
  }

  size_t uuid_count = data.size() / uuid_size;
  for (size_t i = 0; i < uuid_count; i++) {
    const BufferView uuid_bytes(data.data() + (i * uuid_size), uuid_size);
    UUID uuid;
    if (!UUID::FromBytes(uuid_bytes, &uuid) || !func(uuid)) {
      return false;
    }
  }

  return true;
}

UUIDElemSize SizeForType(DataType type) {
  switch (type) {
    case DataType::kIncomplete16BitServiceUuids:
    case DataType::kComplete16BitServiceUuids:
    case DataType::kServiceData16Bit:
      return UUIDElemSize::k16Bit;
    case DataType::kIncomplete32BitServiceUuids:
    case DataType::kComplete32BitServiceUuids:
    case DataType::kServiceData32Bit:
      return UUIDElemSize::k32Bit;
    case DataType::kIncomplete128BitServiceUuids:
    case DataType::kComplete128BitServiceUuids:
    case DataType::kServiceData128Bit:
      return UUIDElemSize::k128Bit;
    default:
      break;
  };

  BT_PANIC("called SizeForType with non-UUID DataType %du",
           static_cast<uint8_t>(type));
  return UUIDElemSize::k16Bit;
}

SupplementDataReader::SupplementDataReader(const ByteBuffer& data)
    : is_valid_(true), remaining_(data) {
  if (!remaining_.size()) {
    is_valid_ = false;
    return;
  }

  // Do a validity check.
  BufferView tmp(remaining_);
  while (tmp.size()) {
    size_t tlv_len = tmp[0];

    // A struct can have 0 as its length. In that case its valid to terminate.
    if (!tlv_len)
      break;

    // The full struct includes the length octet itself.
    size_t struct_size = tlv_len + 1;
    if (struct_size > tmp.size()) {
      is_valid_ = false;
      break;
    }

    tmp = tmp.view(struct_size);
  }
}

bool SupplementDataReader::GetNextField(DataType* out_type,
                                        BufferView* out_data) {
  BT_DEBUG_ASSERT(out_type);
  BT_DEBUG_ASSERT(out_data);

  if (!HasMoreData())
    return false;

  size_t tlv_len = remaining_[0];
  size_t cur_struct_size = tlv_len + 1;
  BT_DEBUG_ASSERT(cur_struct_size <= remaining_.size());

  *out_type = static_cast<DataType>(remaining_[1]);
  *out_data = remaining_.view(2, tlv_len - 1);

  // Update |remaining_|.
  remaining_ = remaining_.view(cur_struct_size);
  return true;
}

bool SupplementDataReader::HasMoreData() const {
  if (!is_valid_ || !remaining_.size())
    return false;

  // If the buffer is valid and has remaining bytes but the length of the next
  // segment is zero, then we terminate.
  return !!remaining_[0];
}

SupplementDataWriter::SupplementDataWriter(MutableByteBuffer* buffer)
    : buffer_(buffer), bytes_written_(0u) {
  BT_DEBUG_ASSERT(buffer_);
}

bool SupplementDataWriter::WriteField(DataType type, const ByteBuffer& data) {
  size_t next_size = data.size() + 2;  // 2 bytes for [length][type].
  if (bytes_written_ + next_size > buffer_->size() || next_size > 255)
    return false;

  (*buffer_)[bytes_written_++] = static_cast<uint8_t>(next_size) - 1;
  (*buffer_)[bytes_written_++] = static_cast<uint8_t>(type);

  // Get a view into the offset we want to write to.
  auto target = buffer_->mutable_view(bytes_written_);

  // Copy the data into the view.
  data.Copy(&target);

  bytes_written_ += data.size();

  return true;
}

}  // namespace bt
