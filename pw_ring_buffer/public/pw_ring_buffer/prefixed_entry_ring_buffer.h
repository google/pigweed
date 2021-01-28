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

#include "pw_containers/intrusive_list.h"
#include "pw_status/status.h"

namespace pw {
namespace ring_buffer {

// A circular ring buffer for arbitrary length data entries. Each PushBack()
// produces a buffer entry. Each entry consists of a preamble followed by an
// arbitrary length data chunk. The preamble is comprised of an optional user
// preamble byte and an always present varint. The varint encodes the number of
// bytes in the data chunk. This is a FIFO queue, with the oldest entries at
// the 'front' (to be processed by readers) and the newest entries at the 'back'
// (where the writer pushes to).
//
// The ring buffer supports multiple readers, which can be attached/detached
// from the buffer. Each reader has its own read pointer and can peek and pop
// the entry at the head. Entries are not bumped out from the buffer until all
// readers have moved past that entry, or if the buffer is at capacity and space
// is needed to push a new entry. When making space, the buffer will push slow
// readers forward to the new oldest entry. Entries are internally wrapped
// around as needed.
class PrefixedEntryRingBufferMulti {
 public:
  typedef Status (*ReadOutput)(std::span<const std::byte>);

  // A reader that provides a single-reader interface into the multi-reader ring
  // buffer it has been attached to via AttachReader(). Readers maintain their
  // read position in the ring buffer as well as the remaining count of entries
  // from that position. Readers are only able to consume entries that were
  // pushed after the attach operation.
  //
  // Readers can peek and pop entries similar to the single-reader interface.
  // When popping entries, although the reader moves forward and drops the
  // entry, the entry is not removed from the ring buffer until all other
  // attached readers have moved past that entry.
  //
  // When the attached ring buffer needs to make space, it may push the reader
  // index forward. Users of this class should consider the possibility of data
  // loss if they read slower than the writer.
  class Reader : public IntrusiveList<Reader>::Item {
   public:
    constexpr Reader() : buffer(nullptr), read_idx(0), entry_count(0) {}

    // TODO(pwbug/344): Add locking to the internal functions. Who owns the
    // lock? This class? Does this class need a lock if it's not a multi-reader?
    // (One doesn't exist today but presumably nothing prevents push + pop
    // operations from happening on two different threads).

    // Read the oldest stored data chunk of data from the ring buffer to
    // the provided destination std::span. The number of bytes read is written
    // to bytes_read
    //
    // Return values:
    // OK - Data successfully read from the ring buffer.
    // FAILED_PRECONDITION - Buffer not initialized.
    // OUT_OF_RANGE - No entries in ring buffer to read.
    // RESOURCE_EXHAUSTED - Destination data std::span was smaller number of
    // bytes than the data size of the data chunk being read.  Available
    // destination bytes were filled, remaining bytes of the data chunk were
    // ignored.
    Status PeekFront(std::span<std::byte> data, size_t* bytes_read_out) {
      return buffer->InternalPeekFront(*this, data, bytes_read_out);
    }

    Status PeekFront(ReadOutput output) {
      return buffer->InternalPeekFront(*this, output);
    }

    // Same as PeekFront but includes the entry's preamble of optional user
    // value and the varint of the data size.
    // TODO(pwbug/341): Move all other APIs to passing bytes_read by reference,
    // as it is required to determine the length populated in the span.
    Status PeekFrontWithPreamble(std::span<std::byte> data,
                                 uint32_t& user_preamble_out,
                                 size_t& entry_bytes_read_out);

    Status PeekFrontWithPreamble(std::span<std::byte> data,
                                 size_t* bytes_read_out) {
      return buffer->InternalPeekFrontWithPreamble(*this, data, bytes_read_out);
    }

    Status PeekFrontWithPreamble(ReadOutput output) {
      return buffer->InternalPeekFrontWithPreamble(*this, output);
    }

    // Pop and discard the oldest stored data chunk of data from the ring
    // buffer.
    //
    // Return values:
    // OK - Data successfully read from the ring buffer.
    // FAILED_PRECONDITION - Buffer not initialized.
    // OUT_OF_RANGE - No entries in ring buffer to pop.
    Status PopFront() { return buffer->InternalPopFront(*this); }

    // Get the size in bytes of the next chunk, not including preamble, to be
    // read.
    size_t FrontEntryDataSizeBytes() {
      return buffer->InternalFrontEntryDataSizeBytes(*this);
    }

    // Get the size in bytes of the next chunk, including preamble and data
    // chunk, to be read.
    size_t FrontEntryTotalSizeBytes() {
      return buffer->InternalFrontEntryTotalSizeBytes(*this);
    }

    // Get the number of variable-length entries currently in the ring buffer.
    //
    // Return value:
    // Entry count.
    size_t EntryCount() { return entry_count; }

   protected:
    friend PrefixedEntryRingBufferMulti;

    PrefixedEntryRingBufferMulti* buffer;
    size_t read_idx;
    size_t entry_count;
  };

  // TODO(pwbug/340): Consider changing bool to an enum, to explicitly enumerate
  // what this variable means in clients.
  PrefixedEntryRingBufferMulti(bool user_preamble = false)
      : buffer_(nullptr),
        buffer_bytes_(0),
        write_idx_(0),
        user_preamble_(user_preamble) {}

  // Set the raw buffer to be used by the ring buffer.
  //
  // Return values:
  // OK - successfully set the raw buffer.
  // INVALID_ARGUMENT - Argument was nullptr, size zero, or too large.
  Status SetBuffer(std::span<std::byte> buffer);

  // Attach reader to the ring buffer. Readers can only be attached to one
  // ring buffer at a time.
  //
  // Return values:
  // OK - Successfully configured reader for ring buffer.
  // INVALID_ARGUMENT - Argument was already attached to another ring buffer.
  Status AttachReader(Reader& reader);

  // Detach reader from the ring buffer. Readers can only be detached if they
  // were previously attached.
  //
  // Return values:
  // OK - Successfully removed reader for ring buffer.
  // INVALID_ARGUMENT - Argument was not previously attached to this ring
  // buffer.
  Status DetachReader(Reader& reader);

  // Removes all data from the ring buffer.
  void Clear();

  // Write a chunk of data to the ring buffer. If available space is less than
  // size of data chunk to be written then silently pop and discard oldest
  // stored data chunks until space is available.
  //
  // Preamble argument is a caller-provided value prepended to the front of the
  // entry. It is only used if user_preamble was set at class construction
  // time. It is varint-encoded before insertion into the buffer.
  //
  // Return values:
  // OK - Data successfully written to the ring buffer.
  // INVALID_ARGUMENT - Size of data to write is zero bytes
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - Size of data is greater than buffer size.
  Status PushBack(std::span<const std::byte> data,
                  uint32_t user_preamble_data = 0) {
    return InternalPushBack(data, user_preamble_data, true);
  }

  // [Deprecated] An implementation of PushBack that accepts a single-byte as
  // preamble data. Clients should migrate to passing uint32_t preamble data.
  Status PushBack(std::span<const std::byte> data,
                  std::byte user_preamble_data) {
    return PushBack(data, static_cast<uint32_t>(user_preamble_data));
  }

  // Write a chunk of data to the ring buffer if there is space available.
  //
  // Preamble argument is a caller-provided value prepended to the front of the
  // entry. It is only used if user_preamble was set at class construction
  // time. It is varint-encoded before insertion into the buffer.
  //
  // Return values:
  // OK - Data successfully written to the ring buffer.
  // INVALID_ARGUMENT - Size of data to write is zero bytes
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - Size of data is greater than buffer size.
  // RESOURCE_EXHAUSTED - The ring buffer doesn't have space for the data
  // without popping off existing elements.
  Status TryPushBack(std::span<const std::byte> data,
                     uint32_t user_preamble_data = 0) {
    return InternalPushBack(data, user_preamble_data, false);
  }

  // [Deprecated] An implementation of TryPushBack that accepts a single-byte as
  // preamble data. Clients should migrate to passing uint32_t preamble data.
  Status TryPushBack(std::span<const std::byte> data,
                     std::byte user_preamble_data) {
    return TryPushBack(data, static_cast<uint32_t>(user_preamble_data));
  }

  // Get the size in bytes of all the current entries in the ring buffer,
  // including preamble and data chunk.
  size_t TotalUsedBytes() { return buffer_bytes_ - RawAvailableBytes(); }

  // Dering the buffer by reordering entries internally in the buffer by
  // rotating to have the oldest entry is at the lowest address/index with
  // newest entry at the highest address.
  //
  // Return values:
  // OK - Buffer data successfully deringed.
  // FAILED_PRECONDITION - Buffer not initialized, or no readers attached.
  Status Dering();

 protected:
  // Read the oldest stored data chunk of data from the ring buffer to
  // the provided destination std::span. The number of bytes read is written to
  // `bytes_read_out`.
  //
  // Return values:
  // OK - Data successfully read from the ring buffer.
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - No entries in ring buffer to read.
  // RESOURCE_EXHAUSTED - Destination data std::span was smaller number of bytes
  // than the data size of the data chunk being read.  Available destination
  // bytes were filled, remaining bytes of the data chunk were ignored.
  Status InternalPeekFront(Reader& reader,
                           std::span<std::byte> data,
                           size_t* bytes_read_out);
  Status InternalPeekFront(Reader& reader, ReadOutput output);

  // Same as Read but includes the entry's preamble of optional user value and
  // the varint of the data size
  Status InternalPeekFrontWithPreamble(Reader& reader,
                                       std::span<std::byte> data,
                                       size_t* bytes_read_out);
  Status InternalPeekFrontWithPreamble(Reader& reader, ReadOutput output);

  // Pop and discard the oldest stored data chunk of data from the ring buffer.
  //
  // Return values:
  // OK - Data successfully read from the ring buffer.
  // FAILED_PRECONDITION - Buffer not initialized.
  // OUT_OF_RANGE - No entries in ring buffer to pop.
  Status InternalPopFront(Reader& reader);

  // Get the size in bytes of the next chunk, not including preamble, to be
  // read.
  size_t InternalFrontEntryDataSizeBytes(Reader& reader);

  // Get the size in bytes of the next chunk, including preamble and data
  // chunk, to be read.
  size_t InternalFrontEntryTotalSizeBytes(Reader& reader);

  // Internal version of Read used by all the public interface versions. T
  // should be of type ReadOutput.
  template <typename T>
  Status InternalRead(Reader& reader,
                      T read_output,
                      bool include_preamble_in_output,
                      uint32_t* user_preamble_out = nullptr);

 private:
  struct EntryInfo {
    size_t preamble_bytes;
    uint32_t user_preamble;
    size_t data_bytes;
  };

  // Push back implementation, which optionally discards front elements to fit
  // the incoming element.
  Status InternalPushBack(std::span<const std::byte> data,
                          uint32_t user_preamble_data,
                          bool pop_front_if_needed);

  // Internal function to pop all of the slowest readers. This function may pop
  // multiple readers if multiple are slow.
  //
  // Precondition: This function requires that at least one reader is attached
  // and has at least one entry to pop.
  void InternalPopFrontAll();

  // Returns the slowest reader in the list.
  //
  // Precondition: This function requires that at least one reader is attached.
  Reader& GetSlowestReader();

  // Get info struct with the size of the preamble and data chunk for the next
  // entry to be read.
  EntryInfo FrontEntryInfo(Reader& reader);

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
  const bool user_preamble_;

  // List of attached readers.
  IntrusiveList<Reader> readers_;

  // Maximum bufer size allowed. Restricted to this to allow index aliasing to
  // not overflow.
  static constexpr size_t kMaxBufferBytes =
      std::numeric_limits<size_t>::max() / 2;
};

class PrefixedEntryRingBuffer : public PrefixedEntryRingBufferMulti,
                                public PrefixedEntryRingBufferMulti::Reader {
 public:
  PrefixedEntryRingBuffer(bool user_preamble = false)
      : PrefixedEntryRingBufferMulti(user_preamble) {
    AttachReader(*this);
  }
};

}  // namespace ring_buffer
}  // namespace pw
