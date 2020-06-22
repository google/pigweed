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
#pragma once

#include <cstddef>
#include <span>

#include "pw_status/status.h"

namespace pw {
namespace ring_buffer {

// A circular ring buffer for arbitrary length data entries. Each PushBack()
// produces a buffer entry. Each entry consists of a preamble followed by an
// arbitrary length data chunk. The preamble is comprised of an optional user
// preamble byte and an always present varint. The varint encodes the number of
// bytes in the data chunk.
//
// The ring buffer holds the most recent entries stored in the buffer. Once
// filled to capacity, incoming entries bump out the oldest entries to make
// room. Entries are internally wrapped around as needed.
class PrefixedEntryRingBuffer {
 public:
  typedef Status (*ReadOutput)(std::span<const std::byte>);

  PrefixedEntryRingBuffer(bool user_preamble = false)
      : buffer_(nullptr),
        buffer_bytes_(0),
        write_idx_(0),
        read_idx_(0),
        entry_count_(0),
        user_preamble_(user_preamble) {}

  // Set the raw buffer to be used by the ring buffer.
  //
  // Return values:
  // OK - successfully set the raw buffer.
  // INVALID_ARGUMENT - Argument was nullptr, size zero, or too large.
  Status SetBuffer(std::span<std::byte> buffer);

  // Removes all data from the ring buffer.
  void Clear();

  // Write a chunk of data to the ring buffer. If available space is less than
  // size of data chunk to be written then silently pop and discard oldest
  // stored data chunks until space is available.
  //
  // Preamble argument is a caller-provided value prepended to the front of the
  // entry. It is only used if user_preamble was set at class construction
  // time.
  //
  // Return values:
  // OK - Data successfully written to the ring buffer.
  // INVALID_ARGUMENT - Size of data to write is zero bytes
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - Size of data is greater than buffer size.
  Status PushBack(std::span<const std::byte> data,
                  std::byte user_preamble_data = std::byte(0)) {
    return InternalPushBack(data, user_preamble_data, true);
  }

  // Write a chunk of data to the ring buffer if there is space available.
  //
  // Preamble argument is a caller-provided value prepended to the front of the
  // entry. It is only used if user_preamble was set at class construction
  // time.
  //
  // Return values:
  // OK - Data successfully written to the ring buffer.
  // INVALID_ARGUMENT - Size of data to write is zero bytes
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - Size of data is greater than buffer size.
  // RESOURCE_EXHAUSTED - The ring buffer doesn't have space for the data
  // without popping off existing elements.
  Status TryPushBack(std::span<const std::byte> data,
                     std::byte user_preamble_data = std::byte(0)) {
    return InternalPushBack(data, user_preamble_data, false);
  }

  // Read the oldest stored data chunk of data from the ring buffer to
  // the provided destination std::span. The number of bytes read is written to
  // bytes_read
  //
  // Return values:
  // OK - Data successfully read from the ring buffer.
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - No entries in ring buffer to read.
  // RESOURCE_EXHAUSTED - Destination data std::span was smaller number of bytes
  // than the data size of the data chunk being read.  Available destination
  // bytes were filled, remaining bytes of the data chunk were ignored.
  Status PeekFront(std::span<std::byte> data, size_t* bytes_read);

  Status PeekFront(ReadOutput output);

  // Same as Read but includes the entry's preamble of optional user value and
  // the varint of the data size
  Status PeekFrontWithPreamble(std::span<std::byte> data, size_t* bytes_read);

  Status PeekFrontWithPreamble(ReadOutput output);

  // Pop and discard the oldest stored data chunk of data from the ring buffer.
  //
  // Return values:
  // OK - Data successfully read from the ring buffer.
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - No entries in ring buffer to pop.
  Status PopFront();

  // Dering the buffer by reordering entries internally in the buffer by
  // rotating to have the oldest entry is at the lowest address/index with
  // newest entry at the highest address.
  //
  // Return values:
  // OK - Buffer data successfully deringed.
  // FAILED_PRECONDITION - Buffer not initialized.
  Status Dering();

  // Get the number of variable-length entries currently in the ring buffer.
  //
  // Return value:
  // Entry count.
  size_t EntryCount() { return entry_count_; }

  // Get the size in bytes of all the current entries in the ring buffer,
  // including preamble and data chunk.
  size_t TotalUsedBytes() { return buffer_bytes_ - RawAvailableBytes(); }

  // Get the size in bytes of the next chunk, not including preamble, to be
  // read.
  size_t FrontEntryDataSizeBytes();

  // Get the size in bytes of the next chunk, including preamble and data
  // chunk, to be read.
  size_t FrontEntryTotalSizeBytes();

 private:
  struct EntryInfo {
    size_t preamble_bytes;
    size_t data_bytes;
  };

  // Internal version of Read used by all the public interface versions. T
  // should be of type ReadOutput.
  template <typename T>
  Status InternalRead(T read_output, bool get_preamble);

  // Push back implementation, which optionally discards front elements to fit
  // the incoming element.
  Status InternalPushBack(std::span<const std::byte> data,
                          std::byte user_preamble_data,
                          bool pop_front_if_needed);

  // Get info struct with the size of the preamble and data chunk for the next
  // entry to be read.
  EntryInfo FrontEntryInfo();

  // Get the raw number of available bytes free in the ring buffer. This is
  // not available bytes for data, since there is a variable size preamble for
  // each entry.
  size_t RawAvailableBytes();

  // Do the basic write of the specified number of bytes starting at the last
  // write index of the ring buffer to the destination, handing any wrap-around
  // of the ring buffer. This is basic, raw operation with no safety checks.
  void RawWrite(std::span<const std::byte> source);

  // Do the basic read of the specified number of bytes starting at the given
  // index of the ring buffer to the destination, handing any wrap-around of
  // the ring buffer. This is basic, raw operation with no safety checks.
  void RawRead(std::byte* destination, size_t source_idx, size_t length_bytes);

  size_t IncrementIndex(size_t index, size_t count);

  std::byte* buffer_;
  size_t buffer_bytes_;

  size_t write_idx_;
  size_t read_idx_;
  size_t entry_count_;
  const bool user_preamble_;

  // Worst case size for the variable-sized preable that is prepended to
  // each entry.
  static constexpr size_t kMaxEntryPreambleBytes = sizeof(size_t) + 1;

  // Maximum bufer size allowed. Restricted to this to allow index aliasing to
  // not overflow.
  static constexpr size_t kMaxBufferBytes =
      std::numeric_limits<size_t>::max() / 2;
};

}  // namespace ring_buffer
}  // namespace pw
