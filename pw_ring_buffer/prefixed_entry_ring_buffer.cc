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

#include "pw_ring_buffer/prefixed_entry_ring_buffer.h"

#include <cstring>

#include "pw_varint/varint.h"

namespace pw {
namespace ring_buffer {

using std::byte;

void PrefixedEntryRingBuffer::Clear() {
  read_idx_ = 0;
  write_idx_ = 0;
  entry_count_ = 0;
}

Status PrefixedEntryRingBuffer::SetBuffer(std::span<byte> buffer) {
  if ((buffer.data() == nullptr) ||  //
      (buffer.size_bytes() == 0) ||  //
      (buffer.size_bytes() > kMaxBufferBytes)) {
    return Status::InvalidArgument();
  }

  buffer_ = buffer.data();
  buffer_bytes_ = buffer.size_bytes();

  Clear();
  return Status::Ok();
}

Status PrefixedEntryRingBuffer::InternalPushBack(std::span<const byte> data,
                                                 byte user_preamble_data,
                                                 bool drop_elements_if_needed) {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (data.size_bytes() == 0) {
    return Status::InvalidArgument();
  }

  // Prepare the preamble, and ensure we can fit the preamble and entry.
  byte varint_buf[kMaxEntryPreambleBytes];
  size_t varint_bytes = varint::Encode<size_t>(data.size_bytes(), varint_buf);
  size_t total_write_bytes =
      (user_preamble_ ? 1 : 0) + varint_bytes + data.size_bytes();
  if (buffer_bytes_ < total_write_bytes) {
    return Status::OutOfRange();
  }

  if (drop_elements_if_needed) {
    // PushBack() case: evict items as needed.
    // Drop old entries until we have space for the new entry.
    while (RawAvailableBytes() < total_write_bytes) {
      PopFront();
    }
  } else if (RawAvailableBytes() < total_write_bytes) {
    // TryPushBack() case: don't evict items.
    return Status::ResourceExhausted();
  }

  // Write the new entry into the ring buffer.
  if (user_preamble_) {
    RawWrite(std::span(&user_preamble_data, sizeof(user_preamble_data)));
  }
  RawWrite(std::span(varint_buf, varint_bytes));
  RawWrite(data);
  entry_count_++;
  return Status::Ok();
}

auto GetOutput(std::span<byte> data_out, size_t* write_index) {
  return [data_out, write_index](std::span<const byte> src) -> Status {
    size_t copy_size = std::min(data_out.size_bytes(), src.size_bytes());

    memcpy(data_out.data() + *write_index, src.data(), copy_size);
    *write_index += copy_size;

    return (copy_size == src.size_bytes()) ? Status::Ok()
                                           : Status::ResourceExhausted();
  };
}

Status PrefixedEntryRingBuffer::PeekFront(std::span<byte> data,
                                          size_t* bytes_read) {
  *bytes_read = 0;
  return InternalRead(GetOutput(data, bytes_read), false);
}

Status PrefixedEntryRingBuffer::PeekFront(ReadOutput output) {
  return InternalRead(output, false);
}

Status PrefixedEntryRingBuffer::PeekFrontWithPreamble(std::span<byte> data,
                                                      size_t* bytes_read) {
  *bytes_read = 0;
  return InternalRead(GetOutput(data, bytes_read), true);
}

Status PrefixedEntryRingBuffer::PeekFrontWithPreamble(ReadOutput output) {
  return InternalRead(output, true);
}

// T should be similar to Status (*read_output)(std::span<const byte>)
template <typename T>
Status PrefixedEntryRingBuffer::InternalRead(T read_output, bool get_preamble) {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (EntryCount() == 0) {
    return Status::OutOfRange();
  }

  // Figure out where to start reading (wrapped); accounting for preamble.
  EntryInfo info = FrontEntryInfo();
  size_t read_bytes = info.data_bytes;
  size_t data_read_idx = read_idx_;
  if (get_preamble) {
    read_bytes += info.preamble_bytes;
  } else {
    data_read_idx = IncrementIndex(data_read_idx, info.preamble_bytes);
  }

  // Read bytes, stopping at the end of the buffer if this entry wraps.
  size_t bytes_until_wrap = buffer_bytes_ - data_read_idx;
  size_t bytes_to_copy = std::min(read_bytes, bytes_until_wrap);
  Status status =
      read_output(std::span(buffer_ + data_read_idx, bytes_to_copy));

  // If the entry wrapped, read the remaining bytes.
  if (status.ok() && (bytes_to_copy < read_bytes)) {
    status = read_output(std::span(buffer_, read_bytes - bytes_to_copy));
  }
  return status;
}

Status PrefixedEntryRingBuffer::PopFront() {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (EntryCount() == 0) {
    return Status::OutOfRange();
  }

  // Advance the read pointer past the front entry to the next one.
  EntryInfo info = FrontEntryInfo();
  size_t entry_bytes = info.preamble_bytes + info.data_bytes;
  read_idx_ = IncrementIndex(read_idx_, entry_bytes);
  entry_count_--;
  return Status::Ok();
}

Status PrefixedEntryRingBuffer::Dering() {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  // Check if by luck we're already deringed.
  if (read_idx_ == 0) {
    return Status::Ok();
  }

  auto buffer_span = std::span(buffer_, buffer_bytes_);
  std::rotate(
      buffer_span.begin(), buffer_span.begin() + read_idx_, buffer_span.end());

  // If the new index is past the end of the buffer,
  // alias it back (wrap) to the start of the buffer.
  if (write_idx_ < read_idx_) {
    write_idx_ += buffer_bytes_;
  }
  write_idx_ -= read_idx_;
  read_idx_ = 0;
  return Status::Ok();
}

size_t PrefixedEntryRingBuffer::FrontEntryDataSizeBytes() {
  if (EntryCount() == 0) {
    return 0;
  }
  return FrontEntryInfo().data_bytes;
}

size_t PrefixedEntryRingBuffer::FrontEntryTotalSizeBytes() {
  if (EntryCount() == 0) {
    return 0;
  }
  EntryInfo info = FrontEntryInfo();
  return info.preamble_bytes + info.data_bytes;
}

PrefixedEntryRingBuffer::EntryInfo PrefixedEntryRingBuffer::FrontEntryInfo() {
  // Entry headers consists of: (optional prefix byte, varint size, data...)

  // Read the entry header; extract the varint and it's size in bytes.
  byte varint_buf[kMaxEntryPreambleBytes];
  RawRead(varint_buf,
          IncrementIndex(read_idx_, user_preamble_ ? 1 : 0),
          kMaxEntryPreambleBytes);
  uint64_t entry_size;
  size_t varint_size = varint::Decode(varint_buf, &entry_size);

  EntryInfo info = {};
  info.preamble_bytes = (user_preamble_ ? 1 : 0) + varint_size;
  info.data_bytes = entry_size;
  return info;
}

// Comparisons ordered for more probable early exits, assuming the reader is
// not far behind the writer compared to the size of the ring.
size_t PrefixedEntryRingBuffer::RawAvailableBytes() {
  // Case: Not wrapped.
  if (read_idx_ < write_idx_) {
    return buffer_bytes_ - (write_idx_ - read_idx_);
  }
  // Case: Wrapped
  if (read_idx_ > write_idx_) {
    return read_idx_ - write_idx_;
  }
  // Case: Matched read and write heads; empty or full.
  return entry_count_ ? 0 : buffer_bytes_;
}

void PrefixedEntryRingBuffer::RawWrite(std::span<const std::byte> source) {
  // Write until the end of the source or the backing buffer.
  size_t bytes_until_wrap = buffer_bytes_ - write_idx_;
  size_t bytes_to_copy = std::min(source.size(), bytes_until_wrap);
  memcpy(buffer_ + write_idx_, source.data(), bytes_to_copy);

  // If there wasn't space in the backing buffer, wrap to the front.
  if (bytes_to_copy < source.size()) {
    memcpy(
        buffer_, source.data() + bytes_to_copy, source.size() - bytes_to_copy);
  }
  write_idx_ = IncrementIndex(write_idx_, source.size());
}

void PrefixedEntryRingBuffer::RawRead(byte* destination,
                                      size_t source_idx,
                                      size_t length_bytes) {
  // Read the pre-wrap bytes.
  size_t bytes_until_wrap = buffer_bytes_ - source_idx;
  size_t bytes_to_copy = std::min(length_bytes, bytes_until_wrap);
  memcpy(destination, buffer_ + source_idx, bytes_to_copy);

  // Read the post-wrap bytes, if needed.
  if (bytes_to_copy < length_bytes) {
    memcpy(destination + bytes_to_copy, buffer_, length_bytes - bytes_to_copy);
  }
}

size_t PrefixedEntryRingBuffer::IncrementIndex(size_t index, size_t count) {
  // Note: This doesn't use modulus (%) since the branch is cheaper, and we
  // guarantee that count will never be greater than buffer_bytes_.
  index += count;
  if (index > buffer_bytes_) {
    index -= buffer_bytes_;
  }
  return index;
}

}  // namespace ring_buffer
}  // namespace pw
