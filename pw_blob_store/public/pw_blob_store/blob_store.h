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
    constexpr BlobWriter(BlobStore& store) : store_(store), open_(false) {}
    BlobWriter(const BlobWriter&) = delete;
    BlobWriter& operator=(const BlobWriter&) = delete;
    ~BlobWriter() {
      if (open_) {
        Close();
      }
    }

    // Open a blob for writing/erasing. Can not open when already open. Only one
    // writer is allowed to be open at a time. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to open, another writer or reader instance is
    //     already open.
    Status Open() {
      PW_DCHECK(!open_);
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
      PW_DCHECK(open_);
      open_ = false;
      return store_.CloseWrite();
    }

    // Erase the partition and reset state for a new blob. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to erase while reader is open.
    // [error status] - flash erase failed.
    Status Erase() {
      PW_DCHECK(open_);
      return Status::UNIMPLEMENTED;
    }

    // Discard blob (in-progress or valid stored). Any written bytes are
    // considered invalid. Returns:
    //
    // OK - success.
    // FAILED_PRECONDITION - not open.
    Status Discard() {
      PW_DCHECK(open_);
      return store_.Invalidate();
    }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be written. Returns zero if, in the current state, Write would return
    // status other than OK. See stream.h for additional details.
    size_t ConservativeWriteLimit() const override {
      PW_DCHECK(open_);
      return store_.WriteBytesRemaining();
    }

    size_t CurrentSizeBytes() {
      PW_DCHECK(open_);
      return store_.write_address_;
    }

   private:
    Status DoWrite(ConstByteSpan data) override {
      PW_DCHECK(open_);
      return store_.Write(data);
    }

    BlobStore& store_;
    bool open_;
  };

  // Implement stream::Reader interface for BlobStore.
  class BlobReader final : public stream::Reader {
   public:
    constexpr BlobReader(BlobStore& store, size_t offset)
        : store_(store), open_(false), offset_(offset) {}
    BlobReader(const BlobReader&) = delete;
    BlobReader& operator=(const BlobReader&) = delete;
    ~BlobReader() {
      if (open_) {
        Close();
      }
    }

    // Open to do a blob read. Can not open when already open. Multiple readers
    // can be open at the same time. Returns:
    //
    // OK - success.
    // UNAVAILABLE - Unable to open, already open.
    Status Open() {
      PW_DCHECK(!open_);
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
      PW_DCHECK(open_);
      open_ = false;
      return store_.CloseRead();
    }

    // Probable (not guaranteed) minimum number of bytes at this time that can
    // be read. Returns zero if, in the current state, Read would return status
    // other than OK. See stream.h for additional details.
    size_t ConservativeReadLimit() const override {
      PW_DCHECK(open_);
      return store_.ReadableDataBytes() - offset_;
    }

    // Get a span with the MCU pointer and size of the data. Returns:
    //
    // OK with span - Valid span respresenting the blob data
    // FAILED_PRECONDITION - Reader not open.
    // UNIMPLEMENTED - Memory mapped access not supported for this blob.
    Result<ByteSpan> GetMemoryMappedBlob() {
      PW_DCHECK(open_);
      return store_.GetMemoryMappedBlob();
    }

   private:
    StatusWithSize DoRead(ByteSpan dest) override {
      PW_DCHECK(open_);
      return store_.Read(offset_, dest);
    }

    BlobStore& store_;
    bool open_;
    size_t offset_;
  };

  BlobStore(std::string_view name,
            kvs::FlashPartition* partition,
            kvs::ChecksumAlgorithm* checksum_algo,
            kvs::KeyValueStore& kvs,
            ByteSpan write_buffer)
      : name_(name),
        partition_(*partition),
        checksum_algo_(checksum_algo),
        kvs_(kvs),
        write_buffer_(write_buffer),
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

  // Is the blob erased and ready to write.
  bool erased() const { return flash_erased_; }

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
  // OK - success.
  // FAILED_PRECONDITION - blob is not open.
  Status CloseWrite();
  Status CloseRead();

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

  // Commit data to flash and update flash_address_ with data bytes written. The
  // only time data_bytes should be manually specified is for a CloseWrite with
  // an unaligned-size chunk remaining in the buffer that has been zero padded
  // to alignment.
  Status CommitToFlash(ConstByteSpan source, size_t data_bytes = 0) {
    if (data_bytes == 0) {
      data_bytes = source.size_bytes();
    }
    flash_erased_ = false;
    valid_data_ = true;
    StatusWithSize result = partition_.Write(flash_address_, source);
    flash_address_ += data_bytes;
    if (checksum_algo_ != nullptr) {
      checksum_algo_->Update(source.first(data_bytes));
    }

    return result.status();
  }

  bool WriteBufferEmpty() { return flash_address_ == write_address_; }

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
  Result<ByteSpan> GetMemoryMappedBlob() const;

  // Current size of blob/readable data, in bytes. For a completed write this is
  // the size of the data blob. For all other cases this is zero bytes.
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

  Result<BlobStore::ChecksumValue> CalculateChecksumFromFlash(
      size_t bytes_to_check);

  const std::string_view MetadataKey() { return name_; }

  // Changes to the metadata format should also get a different key signature to
  // avoid new code improperly reading old format metadata.
  struct BlobMetadata {
    // The checksum of the blob data stored in flash.
    ChecksumValue checksum;

    // Number of blob data bytes stored in flash.
    size_t data_size_bytes;

    // This blob is complete (the write was properly closed).
    bool complete;
  };

  std::string_view name_;
  kvs::FlashPartition& partition_;
  kvs::ChecksumAlgorithm* const checksum_algo_;
  kvs::KeyValueStore& kvs_;
  ByteSpan write_buffer_;

  //
  // Internal state for Blob store
  //
  // TODO: Consolidate blob state to a single struct

  // Initialization has been done.
  bool initialized_;

  // Bytes stored are valid and good.
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
template <size_t kBufferSizeBytes>
class BlobStoreBuffer : public BlobStore {
 public:
  explicit BlobStoreBuffer(std::string_view name,
                           kvs::FlashPartition* partition,
                           kvs::ChecksumAlgorithm* checksum_algo,
                           kvs::KeyValueStore& kvs)
      : BlobStore(name, partition, checksum_algo, kvs, buffer_) {}

 private:
  std::array<std::byte, kBufferSizeBytes> buffer_;
};

}  // namespace pw::blob_store
