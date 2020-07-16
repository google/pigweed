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

#include <span>

#include "pw_kvs/checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::blob_store {

// BlobStore is a storage container for a single blob of data. BlobStore is a
// FlashPartition-backed persistent storage system with integrated data
// integrity checking that serves as a lightweight alternative to a file
// system.
//
// Write and read are only done using the BlobWriter and BlobReader classes.
//
// Once a blob write is closed, reopening followed by a Discard(), Write(), or
// Erase() will discard the previous blob.
//
// Write blob:
//  0) Create BlobWriter instance
//  1) BlobWriter::Open().
//  2) Add data using BlobWriter::Write().
//  3) BlobWriter::Close().
//
// Read blob:
//  0) Create BlobReader instance
//  1) BlobReader::Open().
//  2) Read data using BlobReader::Read() or
//     BlobReader::GetMemoryMappedBlob().
//  3) BlobReader::Close().
class BlobStore {
 public:
  // Implement the stream::Writer and erase interface for a BlobStore. If not
  // already erased, the Write will do any needed erase.
  class BlobWriter final : public stream::Writer {
   public:
    constexpr BlobWriter(BlobStore& store) : store_(store) {}
    BlobWriter(const BlobWriter&) = delete;
    BlobWriter& operator=(const BlobWriter&) = delete;
    ~BlobWriter() { Close(); }

    // Open a blob for writing/erasing. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to open, already open.
    Status Open() { return Status::UNIMPLEMENTED; }

    // Finalize a blob write. Flush all remaining buffered data to storage and
    // store blob metadata. Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - blob is not open.
    Status Close() { return Status::UNIMPLEMENTED; }

    // Erase the partition and reset state for a new blob. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to erase, already open.
    // [error status] - flash erase failed.
    Status Erase() { return Status::UNIMPLEMENTED; }

    // Discard blob (in-progress or valid stored). Any written bytes are
    // considered invalid. Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - not open.
    Status Discard() { return Status::UNIMPLEMENTED; }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be written. Returns zero if, in the current state, Write would return
    // status other than OK. See stream.h for additional details.
    size_t ConservativeWriteLimit() const override {
      return store_.MaxDataSizeBytes() - store_.ReadableDataBytes();
    }

   private:
    Status DoWrite(ConstByteSpan data) override { return store_.Write(data); }

    BlobStore& store_;
  };

  // Implement stream::Reader interface for BlobStore.
  class BlobReader final : public stream::Reader {
   public:
    constexpr BlobReader(BlobStore& store, size_t offset)
        : store_(store), offset_(offset) {}
    BlobReader(const BlobReader&) = delete;
    BlobReader& operator=(const BlobReader&) = delete;
    ~BlobReader() { Close(); }

    // Open to do a blob read. Currently only a single reader at a time is
    // supported. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to open, already open.
    Status Open() { return Status::UNIMPLEMENTED; }

    // Finish reading a blob. Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - blob is not open.
    Status Close() { return Status::UNIMPLEMENTED; }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be read. Returns zero if, in the current state, Read would return status
    // other than OK. See stream.h for additional details.
    size_t ConservativeReadLimit() const override {
      return store_.ReadableDataBytes() - offset_;
    }

    // Get a span with the MCU pointer and size of the data. Returns:
    //
    // OK with span - Valid span respresenting the blob data
    // FAILED_PRECONDITION - Reader not open.
    // UNIMPLEMENTED - Memory mapped access not supported for this blob.
    Result<ByteSpan> GetMemoryMappedBlob() {
      return store_.GetMemoryMappedBlob();
    }

   private:
    StatusWithSize DoRead(ByteSpan dest) override {
      return store_.Read(offset_, dest);
    }

    BlobStore& store_;
    size_t offset_;
  };

  BlobStore(kvs::FlashPartition* partition,
            kvs::ChecksumAlgorithm* checksum_algo,
            ByteSpan write_buffer)
      : partition_(*partition),
        checksum_algo_(checksum_algo),
        write_buffer_(write_buffer),
        state_(BlobStore::State::kInvalidData),
        write_address_(0),
        flush_address_(0) {}

  BlobStore(const BlobStore&) = delete;
  BlobStore& operator=(const BlobStore&) = delete;

  // Initialize the blob instance. Checks if storage is erased or has any stored
  // blob data. Returns:
  //
  // OK - success.
  Status Init();

  // Maximum number of data bytes this BlobStore is able to store.
  size_t MaxDataSizeBytes() const;

 private:
  enum class OpenType {
    // Open for doing read operations.
    kRead,

    // Open for Write operations.
    kWrite,
  };

  // Is the blob erased and ready to write.
  bool erased() const { return state_ == State::kErased; }

  bool IsOpen();

  // Open to do a blob read or write. Returns:
  //
  // OK - success.
  // UNAVAILABLE - Unable to open, already open.
  Status Open(BlobStore::OpenType type);

  // Finalize a blob write. Flush all remaining buffered data to storage and
  // store blob metadata. Returns:
  //
  // OK - success.
  // FAILED_PRECONDITION - blob is not open.
  Status Close();

  // Write/append data to the in-progress blob write. Data is written
  // sequentially, with each append added directly after the previous. Data is
  // not guaranteed to be fully written out to storage on Write return. Returns:
  //
  // OK - successful write/enqueue of data.
  // FAILED_PRECONDITION - blob is not in an open (in-progress) write state.
  // RESOURCE_EXHAUSTED - unable to write all of requested data at this time. No
  //     data written.
  // OUT_OF_RANGE - Writer has been exhausted, similar to EOF. No data written,
  //     no more will be written.
  Status Write(ConstByteSpan data);

  // Read valid data. Attempts to read the lesser of output.size_bytes() or
  // available bytes worth of data. Returns:
  //
  // OK with span of bytes read - success, between 1 and dest.size_bytes() were
  //     read.
  // INVALID_ARGUMENT - offset is invalid.
  // FAILED_PRECONDITION - Reader unable/not in state to read data.
  // RESOURCE_EXHAUSTED - unable to read any bytes at this time. No bytes read.
  //     Try again once bytes become available.
  // OUT_OF_RANGE - Reader has been exhausted, similar to EOF. No bytes read, no
  //     more will be read.
  StatusWithSize Read(size_t offset, ByteSpan dest);

  // Get a span with the MCU pointer and size of the data. Returns:
  //
  // OK with span - Valid span respresenting the blob data
  // FAILED_PRECONDITION - Blob not in a state to read data
  // UNIMPLEMENTED - Memory mapped access not supported for this blob.
  Result<ByteSpan> GetMemoryMappedBlob();

  // Current size of blob/readable data, in bytes. For a completed write this is
  // the size of the data blob. For all other cases this is zero bytes.
  size_t ReadableDataBytes() const;

  Status EraseInternal();

  Status InvalidateInternal();

  kvs::FlashPartition& partition_;
  kvs::ChecksumAlgorithm* const checksum_algo_;
  ByteSpan write_buffer_;
  State state_;
  kvs::FlashPartition::Address write_address_;
  kvs::FlashPartition::Address flush_address_;
};

}  // namespace pw::blob_store
