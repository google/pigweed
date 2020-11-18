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

#include "pw_assert/light.h"
#include "pw_kvs/checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/key_value_store.h"
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
// Once a blob write is closed, reopening to write will discard the previous
// blob.
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
  //
  // Only one writter (of either type) is allowed to be open at a time.
  // Additionally, writters are unable to open if a reader is already open.
  class BlobWriter : public stream::Writer {
   public:
    constexpr BlobWriter(BlobStore& store) : store_(store), open_(false) {}
    BlobWriter(const BlobWriter&) = delete;
    BlobWriter& operator=(const BlobWriter&) = delete;
    virtual ~BlobWriter() {
      if (open_) {
        Close();
      }
    }

    // Open a blob for writing/erasing. Open will invalidate any existing blob
    // that may be stored. Can not open when already open. Only one writer is
    // allowed to be open at a time. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to open, another writer or reader instance is
    //     already open.
    Status Open() {
      PW_DASSERT(!open_);
      Status status = store_.OpenWrite();
      if (status.ok()) {
        open_ = true;
      }
      return status;
    }

    // Finalize a blob write. Flush all remaining buffered data to storage and
    // store blob metadata. Close fails in the closed state, do NOT retry Close
    // on error. An error may or may not result in an invalid blob stored.
    // Returns:
    //
    // OK - success.
    // DATA_LOSS - Error writing data or fail to verify written data.
    Status Close() {
      PW_DASSERT(open_);
      open_ = false;
      return store_.CloseWrite();
    }

    bool IsOpen() { return open_; }

    // Erase the blob partition and reset state for a new blob. Explicit calls
    // to Erase are optional, beginning a write will do any needed Erase.
    // Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to erase while reader is open.
    // [error status] - flash erase failed.
    Status Erase() {
      PW_DASSERT(open_);
      return store_.Erase();
    }

    // Discard the current blob. Any written bytes to this point are considered
    // invalid. Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - not open.
    Status Discard() {
      PW_DASSERT(open_);
      return store_.Invalidate();
    }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be written. This is not necessarily the full number of bytes remaining in
    // the blob. Returns zero if, in the current state, Write would return
    // status other than OK. See stream.h for additional details.
    size_t ConservativeWriteLimit() const override {
      PW_DASSERT(open_);
      return store_.WriteBytesRemaining();
    }

    size_t CurrentSizeBytes() {
      PW_DASSERT(open_);
      return store_.write_address_;
    }

   protected:
    Status DoWrite(ConstByteSpan data) override {
      PW_DASSERT(open_);
      return store_.Write(data);
    }

    BlobStore& store_;
    bool open_;
  };

  // Implement the stream::Writer and erase interface with deferred action for a
  // BlobStore. If not already erased, the Flush will do any needed erase.
  //
  // Only one writter (of either type) is allowed to be open at a time.
  // Additionally, writters are unable to open if a reader is already open.
  class DeferredWriter final : public BlobWriter {
   public:
    constexpr DeferredWriter(BlobStore& store) : BlobWriter(store) {}
    DeferredWriter(const DeferredWriter&) = delete;
    DeferredWriter& operator=(const DeferredWriter&) = delete;
    virtual ~DeferredWriter() {}

    // Flush data in the write buffer. Only a multiple of flash_write_size_bytes
    // are written in the flush. Any remainder is held until later for either
    // a flush with flash_write_size_bytes buffered or the writer is closed.
    Status Flush() {
      PW_DASSERT(open_);
      return store_.Flush();
    }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be written. This is not necessarily the full number of bytes remaining in
    // the blob. Returns zero if, in the current state, Write would return
    // status other than OK. See stream.h for additional details.
    size_t ConservativeWriteLimit() const override {
      PW_DASSERT(open_);
      // Deferred writes need to fit in the write buffer.
      return store_.WriteBufferBytesFree();
    }

   private:
    Status DoWrite(ConstByteSpan data) override {
      PW_DASSERT(open_);
      return store_.AddToWriteBuffer(data);
    }
  };

  // Implement stream::Reader interface for BlobStore. Multiple readers may be
  // open at the same time, but readers may not be open with a writer open.
  class BlobReader final : public stream::Reader {
   public:
    constexpr BlobReader(BlobStore& store)
        : store_(store), open_(false), offset_(0) {}
    BlobReader(const BlobReader&) = delete;
    BlobReader& operator=(const BlobReader&) = delete;
    ~BlobReader() {
      if (open_) {
        Close();
      }
    }

    // Open to do a blob read at the given offset in to the blob. Can not open
    // when already open. Multiple readers can be open at the same time.
    // Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - No readable blob available.
    // INVALID_ARGUMENT - Invalid offset.
    // UNAVAILABLE - Unable to open, already open.
    Status Open(size_t offset = 0) {
      PW_DASSERT(!open_);
      if (!store_.ValidToRead()) {
        return Status::FailedPrecondition();
      }
      if (offset >= store_.ReadableDataBytes()) {
        return Status::InvalidArgument();
      }

      offset_ = offset;
      Status status = store_.OpenRead();
      if (status.ok()) {
        open_ = true;
      }
      return status;
    }

    // Finish reading a blob. Close fails in the closed state, do NOT retry
    // Close on error. Returns:
    //
    // OK - success.
    Status Close() {
      PW_DASSERT(open_);
      open_ = false;
      return store_.CloseRead();
    }

    bool IsOpen() { return open_; }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be read. Returns zero if, in the current state, Read would return status
    // other than OK. See stream.h for additional details.
    size_t ConservativeReadLimit() const override {
      PW_DASSERT(open_);
      return store_.ReadableDataBytes() - offset_;
    }

    // Get a span with the MCU pointer and size of the data. Returns:
    //
    // OK with span - Valid span respresenting the blob data
    // FAILED_PRECONDITION - Reader not open.
    // UNIMPLEMENTED - Memory mapped access not supported for this blob.
    Result<ConstByteSpan> GetMemoryMappedBlob() {
      PW_DASSERT(open_);
      return store_.GetMemoryMappedBlob();
    }

   private:
    StatusWithSize DoRead(ByteSpan dest) override {
      PW_DASSERT(open_);
      StatusWithSize status = store_.Read(offset_, dest);
      if (status.ok()) {
        offset_ += status.size();
      }
      return status;
    }

    BlobStore& store_;
    bool open_;
    size_t offset_;
  };

  // BlobStore
  // name - Name of blob store, used for metadata KVS key
  // partition - Flash partiton to use for this blob. Blob uses the entire
  //     partition for blob data.
  // checksum_algo - Optional checksum for blob integrity checking. Use nullptr
  //     for no check.
  // kvs - KVS used for storing blob metadata.
  // write_buffer - Used for buffering writes. Needs to be at least
  //     flash_write_size_bytes.
  // flash_write_size_bytes - Size in bytes to use for flash write operations.
  //     This should be chosen to balance optimal write size and required buffer
  //     size. Must be greater than or equal to flash write alignment, less than
  //     or equal to flash sector size.
  BlobStore(std::string_view name,
            kvs::FlashPartition& partition,
            kvs::ChecksumAlgorithm* checksum_algo,
            kvs::KeyValueStore& kvs,
            ByteSpan write_buffer,
            size_t flash_write_size_bytes)
      : name_(name),
        partition_(partition),
        checksum_algo_(checksum_algo),
        kvs_(kvs),
        write_buffer_(write_buffer),
        flash_write_size_bytes_(flash_write_size_bytes),
        initialized_(false),
        valid_data_(false),
        flash_erased_(false),
        writer_open_(false),
        readers_open_(0),
        metadata_({}),
        write_address_(0),
        flash_address_(0) {}

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
  typedef uint32_t ChecksumValue;

  Status LoadMetadata();

  // Open to do a blob write. Returns:
  //
  // OK - success.
  // UNAVAILABLE - Unable to open writer, another writer or reader instance is
  //     already open.
  Status OpenWrite();

  // Open to do a blob read. Returns:
  //
  // OK - success.
  // FAILED_PRECONDITION - Unable to open, no valid blob available.
  Status OpenRead();

  // Finalize a blob write. Flush all remaining buffered data to storage and
  // store blob metadata. Returns:
  //
  // OK - success, valid complete blob.
  // DATA_LOSS - Error during write (this close or previous write/flush). Blob
  //     is closed and marked as invalid.
  Status CloseWrite();
  Status CloseRead();

  // Write/append data to the in-progress blob write. Data is written
  // sequentially, with each append added directly after the previous. Data is
  // not guaranteed to be fully written out to storage on Write return. Returns:
  //
  // OK - successful write/enqueue of data.
  // RESOURCE_EXHAUSTED - unable to write all of requested data at this time. No
  //     data written.
  // OUT_OF_RANGE - Writer has been exhausted, similar to EOF. No data written,
  //     no more will be written.
  // DATA_LOSS - Error during write (this write or previous write/flush). No
  //     more will be written by following Write calls for current blob (until
  //     erase/new blob started).
  Status Write(ConstByteSpan data);

  // Similar to Write, but instead immediately writing out to flash, it only
  // buffers the data. A flush or Close is reqired to get bytes writen out to
  // flash
  //
  // OK - successful write/enqueue of data.
  // RESOURCE_EXHAUSTED - unable to write all of requested data at this time. No
  //     data written.
  // OUT_OF_RANGE - Writer has been exhausted, similar to EOF. No data written,
  //     no more will be written.
  // DATA_LOSS - Error during a previous write/flush. No more will be written by
  //     following Write calls for current blob (until erase/new blob started).
  Status AddToWriteBuffer(ConstByteSpan data);

  // Flush data in the write buffer. Only a multiple of flash_write_size_bytes
  // are written in the flush. Any remainder is held until later for either a
  // flush with flash_write_size_bytes buffered or the writer is closed.
  //
  // OK - successful write/enqueue of data.
  // DATA_LOSS - Error during write (this flush or previous write/flush). No
  //     more will be written by following Write calls for current blob (until
  //     erase/new blob started).
  Status Flush();

  // Flush a chunk of data in the write buffer smaller than
  // flash_write_size_bytes. This is only for the final flush as part of the
  // CloseWrite. The partial chunk is padded to flash_write_size_bytes and a
  // flash_write_size_bytes chunk is written to flash.
  //
  // OK - successful write/enqueue of data.
  // DATA_LOSS - Error during write (this flush or previous write/flush). No
  //     more will be written by following Write calls for current blob (until
  //     erase/new blob started).
  Status FlushFinalPartialChunk();

  // Commit data to flash and update flash_address_ with data bytes written. The
  // only time data_bytes should be manually specified is for a CloseWrite with
  // an unaligned-size chunk remaining in the buffer that has been zero padded
  // to alignment.
  Status CommitToFlash(ConstByteSpan source, size_t data_bytes = 0);

  // Blob is valid/OK to write to. Blob is considered valid to write if no data
  // has been written due to the auto/implicit erase on write start.
  //
  // true - Blob is valid and OK to write to.
  // false - Blob has previously had an error and not valid for writing new
  //     data.
  bool ValidToWrite() { return (valid_data_ == true) || (write_address_ == 0); }

  bool WriteBufferEmpty() const { return flash_address_ == write_address_; }

  size_t WriteBufferBytesUsed() const;

  size_t WriteBufferBytesFree() const;

  Status EraseIfNeeded();

  // Blob is valid/OK and has data to read.
  bool ValidToRead() const { return (valid_data_ && ReadableDataBytes() > 0); }

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
  StatusWithSize Read(size_t offset, ByteSpan dest) const;

  // Get a span with the MCU pointer and size of the data. Returns:
  //
  // OK with span - Valid span respresenting the blob data
  // FAILED_PRECONDITION - Blob not in a state to read data
  // UNIMPLEMENTED - Memory mapped access not supported for this blob.
  Result<ConstByteSpan> GetMemoryMappedBlob() const;

  // Size of blob/readable data, in bytes.
  size_t ReadableDataBytes() const;

  size_t WriteBytesRemaining() const {
    return MaxDataSizeBytes() - write_address_;
  }

  Status Erase();

  Status Invalidate();

  void ResetChecksum() {
    if (checksum_algo_ != nullptr) {
      checksum_algo_->Reset();
    }
  }

  Status ValidateChecksum();

  Status CalculateChecksumFromFlash(size_t bytes_to_check);

  const std::string_view MetadataKey() { return name_; }

  // Changes to the metadata format should also get a different key signature to
  // avoid new code improperly reading old format metadata.
  struct BlobMetadata {
    // The checksum of the blob data stored in flash.
    ChecksumValue checksum;

    // Number of blob data bytes stored in flash.
    size_t data_size_bytes;

    constexpr void reset() {
      *this = {
          .checksum = 0,
          .data_size_bytes = 0,
      };
    }
  };

  std::string_view name_;
  kvs::FlashPartition& partition_;
  // checksum_algo_ of nullptr indicates no checksum algorithm.
  kvs::ChecksumAlgorithm* const checksum_algo_;
  kvs::KeyValueStore& kvs_;
  ByteSpan write_buffer_;

  // Size in bytes of flash write operations. This should be chosen to balance
  // optimal write size and required buffer size. Must be GE flash write
  // alignment, LE flash sector size.
  const size_t flash_write_size_bytes_;

  //
  // Internal state for Blob store
  //
  // TODO: Consolidate blob state to a single struct

  // Initialization has been done.
  bool initialized_;

  // Bytes stored are valid and good. Blob is OK to read and write to. Set as
  // soon as blob is erased. Even when bytes written is still 0, they are valid.
  bool valid_data_;

  // Blob partition is currently erased and ready to write a new blob.
  bool flash_erased_;

  // BlobWriter instance is currently open
  bool writer_open_;

  // Count of open BlobReader instances
  size_t readers_open_;

  // Metadata for the blob.
  BlobMetadata metadata_;

  // Current index for end of overal blob data. Represents current byte size of
  // blob data since the FlashPartition starts at address 0.
  kvs::FlashPartition::Address write_address_;

  // Current index of end of data written to flash. Number of buffered data
  // bytes is write_address_ - flash_address_.
  kvs::FlashPartition::Address flash_address_;
};

// Creates a BlobStore with the buffer of kBufferSizeBytes.
//
// kBufferSizeBytes - Size in bytes of write buffer to create.
// name - Name of blob store, used for metadata KVS key
// partition - Flash partiton to use for this blob. Blob uses the entire
//     partition for blob data.
// checksum_algo - Optional checksum for blob integrity checking. Use nullptr
//     for no check.
// kvs - KVS used for storing blob metadata.
// write_buffer - Used for buffering writes. Needs to be at least
//     flash_write_size_bytes.
// flash_write_size_bytes - Size in bytes to use for flash write operations.
//     This should be chosen to balance optimal write size and required buffer
//     size. Must be greater than or equal to flash write alignment, less than
//     or equal to flash sector size.

template <size_t kBufferSizeBytes>
class BlobStoreBuffer : public BlobStore {
 public:
  explicit BlobStoreBuffer(std::string_view name,
                           kvs::FlashPartition& partition,
                           kvs::ChecksumAlgorithm* checksum_algo,
                           kvs::KeyValueStore& kvs,
                           size_t flash_write_size_bytes)
      : BlobStore(name,
                  partition,
                  checksum_algo,
                  kvs,
                  buffer_,
                  flash_write_size_bytes) {}

 private:
  std::array<std::byte, kBufferSizeBytes> buffer_;
};

}  // namespace pw::blob_store
