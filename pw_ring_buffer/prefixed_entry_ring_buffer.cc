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

#include <algorithm>
#include <cstring>

#include "pw_assert/light.h"
#include "pw_varint/varint.h"

namespace pw {
namespace ring_buffer {

using std::byte;
using Reader = PrefixedEntryRingBufferMulti::Reader;

void PrefixedEntryRingBufferMulti::Clear() {
  write_idx_ = 0;
  for (Reader& reader : readers_) {
    reader.read_idx = 0;
    reader.entry_count = 0;
  }
}

Status PrefixedEntryRingBufferMulti::SetBuffer(std::span<byte> buffer) {
  if ((buffer.data() == nullptr) ||  //
      (buffer.size_bytes() == 0) ||  //
      (buffer.size_bytes() > kMaxBufferBytes)) {
    return Status::InvalidArgument();
  }

  buffer_ = buffer.data();
  buffer_bytes_ = buffer.size_bytes();

  Clear();
  return OkStatus();
}

Status PrefixedEntryRingBufferMulti::AttachReader(Reader& reader) {
  if (reader.buffer != nullptr) {
    return Status::InvalidArgument();
  }
  reader.buffer = this;

  // Note that a newly attached reader sees the buffer as empty,
  // and is not privy to entries pushed before being attached.
  reader.read_idx = write_idx_;
  reader.entry_count = 0;
  readers_.push_back(reader);
  return OkStatus();
}

Status PrefixedEntryRingBufferMulti::DetachReader(Reader& reader) {
  if (reader.buffer != this) {
    return Status::InvalidArgument();
  }
  reader.buffer = nullptr;
  reader.read_idx = 0;
  reader.entry_count = 0;
  readers_.remove(reader);
  return OkStatus();
}

Status PrefixedEntryRingBufferMulti::InternalPushBack(
    std::span<const byte> data,
    uint32_t user_preamble_data,
    bool drop_elements_if_needed) {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (data.size_bytes() == 0) {
    return Status::InvalidArgument();
  }

  // Prepare a single buffer that can hold both the user preamble and entry
  // length.
  byte preamble_buf[varint::kMaxVarint32SizeBytes * 2];
  size_t user_preamble_bytes = 0;
  if (user_preamble_) {
    user_preamble_bytes =
        varint::Encode<uint32_t>(user_preamble_data, preamble_buf);
  }
  size_t length_bytes = varint::Encode<uint32_t>(
      data.size_bytes(), std::span(preamble_buf).subspan(user_preamble_bytes));
  size_t total_write_bytes =
      user_preamble_bytes + length_bytes + data.size_bytes();
  if (buffer_bytes_ < total_write_bytes) {
    return Status::OutOfRange();
  }

  if (drop_elements_if_needed) {
    // PushBack() case: evict items as needed.
    // Drop old entries until we have space for the new entry.
    while (RawAvailableBytes() < total_write_bytes) {
      InternalPopFrontAll();
    }
  } else if (RawAvailableBytes() < total_write_bytes) {
    // TryPushBack() case: don't evict items.
    return Status::ResourceExhausted();
  }

  // Write the new entry into the ring buffer.
  RawWrite(std::span(preamble_buf, user_preamble_bytes + length_bytes));
  RawWrite(data);

  // Update all readers of the new count.
  for (Reader& reader : readers_) {
    reader.entry_count++;
  }
  return OkStatus();
}

auto GetOutput(std::span<byte> data_out, size_t* write_index) {
  return [data_out, write_index](std::span<const byte> src) -> Status {
    size_t copy_size = std::min(data_out.size_bytes(), src.size_bytes());

    memcpy(data_out.data() + *write_index, src.data(), copy_size);
    *write_index += copy_size;

    return (copy_size == src.size_bytes()) ? OkStatus()
                                           : Status::ResourceExhausted();
  };
}

Status PrefixedEntryRingBufferMulti::InternalPeekFront(Reader& reader,
                                                       std::span<byte> data,
                                                       size_t* bytes_read_out) {
  *bytes_read_out = 0;
  return InternalRead(reader, GetOutput(data, bytes_read_out), false);
}

Status PrefixedEntryRingBufferMulti::InternalPeekFront(Reader& reader,
                                                       ReadOutput output) {
  return InternalRead(reader, output, false);
}

Status PrefixedEntryRingBufferMulti::InternalPeekFrontWithPreamble(
    Reader& reader, std::span<byte> data, size_t* bytes_read_out) {
  *bytes_read_out = 0;
  return InternalRead(reader, GetOutput(data, bytes_read_out), true);
}

Status PrefixedEntryRingBufferMulti::InternalPeekFrontWithPreamble(
    Reader& reader, ReadOutput output) {
  return InternalRead(reader, output, true);
}

// TODO(pwbug/339): Consider whether this internal templating is required, or if
// we can simply promote GetOutput to a static function and remove the template.
// T should be similar to Status (*read_output)(std::span<const byte>)
template <typename T>
Status PrefixedEntryRingBufferMulti::InternalRead(
    Reader& reader,
    T read_output,
    bool include_preamble_in_output,
    uint32_t* user_preamble_out) {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (reader.entry_count == 0) {
    return Status::OutOfRange();
  }

  // Figure out where to start reading (wrapped); accounting for preamble.
  EntryInfo info = FrontEntryInfo(reader);
  size_t read_bytes = info.data_bytes;
  size_t data_read_idx = reader.read_idx;
  if (user_preamble_out) {
    *user_preamble_out = info.user_preamble;
  }
  if (include_preamble_in_output) {
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

void PrefixedEntryRingBufferMulti::InternalPopFrontAll() {
  // Forcefully pop all readers. Find the slowest reader, which must have
  // the highest entry count, then pop all readers that have the same count.
  //
  // It is expected that InternalPopFrontAll is called only when there is
  // something to pop from at least one reader. If no readers exist, or all
  // readers are caught up, this function will assert.
  size_t entry_count = GetSlowestReader().entry_count;
  PW_DASSERT(entry_count != 0);
  // Otherwise, pop the readers that have the largest value.
  for (Reader& reader : readers_) {
    if (reader.entry_count == entry_count) {
      reader.PopFront();
    }
  }
}

Reader& PrefixedEntryRingBufferMulti::GetSlowestReader() {
  // Readers are guaranteed to be before the writer pointer (the class enforces
  // this on every read/write operation that forces the write pointer ahead of
  // an existing reader). To determine the slowest reader, we consider three
  // scenarios:
  //
  // In all below cases, WH is the write-head, and R# are readers, with R1
  // representing the slowest reader.
  // [[R1 R2 R3 WH]] => Right-hand writer, slowest reader is left-most reader.
  // [[WH R1 R2 R3]] => Left-hand writer, slowest reader is left-most reader.
  // [[R3 WH R1 R2]] => Middle-writer, slowest reader is left-most reader after
  // writer.
  //
  // Formally, choose the left-most reader after the writer (ex.2,3), but if
  // that doesn't exist, choose the left-most reader before the writer (ex.1).
  PW_DASSERT(readers_.size() > 0);
  Reader* slowest_reader_after_writer = nullptr;
  Reader* slowest_reader_before_writer = nullptr;
  for (Reader& reader : readers_) {
    if (reader.read_idx < write_idx_) {
      if (!slowest_reader_before_writer ||
          reader.read_idx < slowest_reader_before_writer->read_idx) {
        slowest_reader_before_writer = &reader;
      }
    } else {
      if (!slowest_reader_after_writer ||
          reader.read_idx < slowest_reader_after_writer->read_idx) {
        slowest_reader_after_writer = &reader;
      }
    }
  }
  return *(slowest_reader_after_writer ? slowest_reader_after_writer
                                       : slowest_reader_before_writer);
}

Status PrefixedEntryRingBufferMulti::Dering() {
  if (buffer_ == nullptr || readers_.size() == 0) {
    return Status::FailedPrecondition();
  }

  // Check if by luck we're already deringed.
  Reader* slowest_reader = &GetSlowestReader();
  if (slowest_reader->read_idx == 0) {
    return OkStatus();
  }

  auto buffer_span = std::span(buffer_, buffer_bytes_);
  std::rotate(buffer_span.begin(),
              buffer_span.begin() + slowest_reader->read_idx,
              buffer_span.end());

  // If the new index is past the end of the buffer,
  // alias it back (wrap) to the start of the buffer.
  if (write_idx_ < slowest_reader->read_idx) {
    write_idx_ += buffer_bytes_;
  }
  write_idx_ -= slowest_reader->read_idx;

  for (Reader& reader : readers_) {
    if (&reader == slowest_reader) {
      continue;
    }
    if (reader.read_idx < slowest_reader->read_idx) {
      reader.read_idx += buffer_bytes_;
    }
    reader.read_idx -= slowest_reader->read_idx;
  }

  slowest_reader->read_idx = 0;
  return OkStatus();
}

Status PrefixedEntryRingBufferMulti::InternalPopFront(Reader& reader) {
  if (buffer_ == nullptr) {
    return Status::FailedPrecondition();
  }
  if (reader.entry_count == 0) {
    return Status::OutOfRange();
  }

  // Advance the read pointer past the front entry to the next one.
  EntryInfo info = FrontEntryInfo(reader);
  size_t entry_bytes = info.preamble_bytes + info.data_bytes;
  size_t prev_read_idx = reader.read_idx;
  reader.read_idx = IncrementIndex(prev_read_idx, entry_bytes);
  reader.entry_count--;
  return OkStatus();
}

size_t PrefixedEntryRingBufferMulti::InternalFrontEntryDataSizeBytes(
    Reader& reader) {
  if (reader.entry_count == 0) {
    return 0;
  }
  return FrontEntryInfo(reader).data_bytes;
}

size_t PrefixedEntryRingBufferMulti::InternalFrontEntryTotalSizeBytes(
    Reader& reader) {
  if (reader.entry_count == 0) {
    return 0;
  }
  EntryInfo info = FrontEntryInfo(reader);
  return info.preamble_bytes + info.data_bytes;
}

PrefixedEntryRingBufferMulti::EntryInfo
PrefixedEntryRingBufferMulti::FrontEntryInfo(Reader& reader) {
  // Entry headers consists of: (optional prefix byte, varint size, data...)

  // If a preamble exists, extract the varint and it's bytes in bytes.
  size_t user_preamble_bytes = 0;
  uint64_t user_preamble_data = 0;
  byte varint_buf[varint::kMaxVarint32SizeBytes];
  if (user_preamble_) {
    RawRead(varint_buf, reader.read_idx, varint::kMaxVarint32SizeBytes);
    user_preamble_bytes = varint::Decode(varint_buf, &user_preamble_data);
    PW_DASSERT(user_preamble_bytes != 0u);
  }

  // Read the entry header; extract the varint and it's bytes in bytes.
  RawRead(varint_buf,
          IncrementIndex(reader.read_idx, user_preamble_bytes),
          varint::kMaxVarint32SizeBytes);
  uint64_t entry_bytes;
  size_t length_bytes = varint::Decode(varint_buf, &entry_bytes);
  PW_DASSERT(length_bytes != 0u);

  EntryInfo info = {};
  info.preamble_bytes = user_preamble_bytes + length_bytes;
  info.user_preamble = static_cast<uint32_t>(user_preamble_data);
  info.data_bytes = entry_bytes;
  return info;
}

// Comparisons ordered for more probable early exits, assuming the reader is
// not far behind the writer compared to the size of the ring.
size_t PrefixedEntryRingBufferMulti::RawAvailableBytes() {
  // Compute slowest reader.
  // TODO: Alternatively, the slowest reader could be actively mantained on
  // every read operation, but reads are more likely than writes.
  if (readers_.size() == 0) {
    return buffer_bytes_;
  }

  size_t read_idx = GetSlowestReader().read_idx;
  // Case: Not wrapped.
  if (read_idx < write_idx_) {
    return buffer_bytes_ - (write_idx_ - read_idx);
  }
  // Case: Wrapped
  if (read_idx > write_idx_) {
    return read_idx - write_idx_;
  }
  // Case: Matched read and write heads; empty or full.
  for (Reader& reader : readers_) {
    if (reader.read_idx == read_idx && reader.entry_count != 0) {
      return 0;
    }
  }
  return buffer_bytes_;
}

void PrefixedEntryRingBufferMulti::RawWrite(std::span<const std::byte> source) {
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

void PrefixedEntryRingBufferMulti::RawRead(byte* destination,
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

size_t PrefixedEntryRingBufferMulti::IncrementIndex(size_t index,
                                                    size_t count) {
  // Note: This doesn't use modulus (%) since the branch is cheaper, and we
  // guarantee that count will never be greater than buffer_bytes_.
  index += count;
  if (index > buffer_bytes_) {
    index -= buffer_bytes_;
  }
  return index;
}

Status PrefixedEntryRingBufferMulti::Reader::PeekFrontWithPreamble(
    std::span<byte> data,
    uint32_t& user_preamble_out,
    size_t& entry_bytes_read_out) {
  entry_bytes_read_out = 0;
  return buffer->InternalRead(
      *this, GetOutput(data, &entry_bytes_read_out), false, &user_preamble_out);
}

}  // namespace ring_buffer
}  // namespace pw
