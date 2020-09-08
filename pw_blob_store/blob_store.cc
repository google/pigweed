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

#include "pw_blob_store/blob_store.h"

#include <algorithm>

#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::blob_store {

Status BlobStore::Init() {
  if (initialized_) {
    return Status::OK;
  }

  const size_t write_buffer_size_alignment =
      write_buffer_.size_bytes() % partition_.alignment_bytes();
  PW_CHECK_UINT_EQ((write_buffer_size_alignment), 0);
  PW_CHECK_UINT_GE(write_buffer_.size_bytes(), partition_.alignment_bytes());
  PW_CHECK_UINT_LE(write_buffer_.size_bytes(), partition_.sector_size_bytes());
  PW_CHECK_UINT_GE(write_buffer_.size_bytes(), flash_write_size_bytes_);
  PW_CHECK_UINT_GE(flash_write_size_bytes_, partition_.alignment_bytes());

  bool erased = false;
  if (partition_.IsErased(&erased).ok() && erased) {
    flash_erased_ = true;
    valid_data_ = true;
  }

  ResetChecksum();

  // TODO: Add checking for already written valid blob.

  initialized_ = true;
  return Status::OK;
}

size_t BlobStore::MaxDataSizeBytes() const { return partition_.size_bytes(); }

Status BlobStore::OpenWrite() {
  // TODO: should Open auto initialize?
  if (!initialized_) {
    return Status::FAILED_PRECONDITION;
  }

  // Writer can only be opened if there are no other writer or readers already
  // open.
  if (writer_open_ || readers_open_ != 0) {
    return Status::UNAVAILABLE;
  }

  writer_open_ = true;

  write_address_ = 0;
  flash_address_ = 0;

  ResetChecksum();

  return Status::OK;
}

Status BlobStore::OpenRead() {
  if (!initialized_) {
    return Status::FAILED_PRECONDITION;
  }

  // If there is no open writer, then once the reader opens there can not be
  // one. So check that there is actully valid data.
  // TODO: Move this validation to Init and CloseWrite
  if (!writer_open_) {
    if (!ValidateChecksum().ok()) {
      PW_LOG_ERROR("Validation check failed, invalidate blob");
      Invalidate();
      return Status::FAILED_PRECONDITION;
    }
  }

  readers_open_++;
  return Status::OK;
}

Status BlobStore::CloseWrite() {
  auto do_close_write = [&]() -> Status {
    // If not valid to write, there was data loss and the close will result in a
    // not valid blob. Don't need to flush any write buffered bytes.
    if (!ValidToWrite()) {
      return Status::DATA_LOSS;
    }

    if (write_address_ == 0) {
      return Status::OK;
    }

    PW_LOG_DEBUG("Close with %u bytes in write buffer",
                 static_cast<unsigned>(WriteBufferBytesUsed()));

    // Do a Flush of any flash_write_size_bytes_ sized chunks so any remaining
    // bytes in the write buffer are less than flash_write_size_bytes_.
    PW_TRY(Flush());

    // If any bytes remain in buffer it is because it is a chunk less than
    // flash_write_size_bytes_. Pad the chunk to flash_write_size_bytes_ and
    // write it to flash.
    if (!WriteBufferEmpty()) {
      PW_TRY(FlushFinalPartialChunk());
    }

    // If things are still good, save the blob metadata.
    // Should not be completing a blob write when it has completed metadata.
    PW_DCHECK(WriteBufferEmpty());
    PW_DCHECK(!metadata_.complete);

    metadata_ = {
        .checksum = 0, .data_size_bytes = flash_address_, .complete = true};
    if (checksum_algo_ != nullptr) {
      ConstByteSpan checksum = checksum_algo_->Finish();
      std::memcpy(&metadata_.checksum,
                  checksum.data(),
                  std::min(checksum.size(), sizeof(metadata_.checksum)));
    }

    PW_TRY(kvs_.Put(MetadataKey(), metadata_));
    PW_TRY(ValidateChecksum());

    return Status::OK;
  };

  const Status status = do_close_write();
  writer_open_ = false;

  if (!status.ok()) {
    valid_data_ = false;
    return Status::DATA_LOSS;
  }
  return Status::OK;
}

Status BlobStore::CloseRead() {
  PW_CHECK_UINT_GT(readers_open_, 0);
  readers_open_--;
  return Status::OK;
}

Status BlobStore::Write(ConstByteSpan data) {
  if (!ValidToWrite()) {
    return Status::DATA_LOSS;
  }
  if (data.size_bytes() == 0) {
    return Status::OK;
  }
  if (WriteBytesRemaining() == 0) {
    return Status::OUT_OF_RANGE;
  }
  if (WriteBytesRemaining() < data.size_bytes()) {
    return Status::RESOURCE_EXHAUSTED;
  }

  Status status = EraseIfNeeded();
  // TODO: switch to TRY once available.
  if (!status.ok()) {
    return Status::DATA_LOSS;
  }

  // Write in (up to) 3 steps:
  // 1) Finish filling write buffer and if full write it to flash.
  // 2) Write as many whole block-sized chunks as the data has remaining
  //    after 1.
  // 3) Put any remaining bytes less than flash write size in the write buffer.

  // Step 1) If there is any data in the write buffer, finish filling write
  //         buffer and if full write it to flash.
  if (!WriteBufferEmpty()) {
    size_t bytes_in_buffer = WriteBufferBytesUsed();

    // Non-deferred writes only use the first flash_write_size_bytes_ of the
    // write buffer to buffer writes less than flash_write_size_bytes_.
    PW_CHECK_UINT_GT(flash_write_size_bytes_, bytes_in_buffer);

    // Not using WriteBufferBytesFree() because non-deferred writes (which
    // is this method) only use the first flash_write_size_bytes_ of the write
    // buffer.
    size_t buffer_remaining = flash_write_size_bytes_ - bytes_in_buffer;

    // Add bytes up to filling the flash write size.
    size_t add_bytes = std::min(buffer_remaining, data.size_bytes());
    std::memcpy(write_buffer_.data() + bytes_in_buffer, data.data(), add_bytes);
    write_address_ += add_bytes;
    bytes_in_buffer += add_bytes;
    data = data.subspan(add_bytes);

    if (bytes_in_buffer != flash_write_size_bytes_) {
      // If there was not enough bytes to finish filling the write buffer, there
      // should not be any bytes left.
      PW_DCHECK(data.size_bytes() == 0);
      return Status::OK;
    }

    // The write buffer is full, flush to flash.
    Status status = CommitToFlash(write_buffer_);
    // TODO: switch to TRY once available.
    if (!status.ok()) {
      return Status::DATA_LOSS;
    }

    PW_DCHECK(WriteBufferEmpty());
  }

  // At this point, if data.size_bytes() > 0, the write buffer should be empty.
  // This invariant is checked as part of of steps 2 & 3.

  // Step 2) Write as many block-sized chunks as the data has remaining after
  //         step 1.
  while (data.size_bytes() >= flash_write_size_bytes_) {
    PW_DCHECK(WriteBufferEmpty());

    write_address_ += flash_write_size_bytes_;
    Status status = CommitToFlash(data.first(flash_write_size_bytes_));
    // TODO: switch to TRY once available.
    if (!status.ok()) {
      return Status::DATA_LOSS;
    }

    data = data.subspan(flash_write_size_bytes_);
  }

  // step 3) Put any remaining bytes to the buffer. Put the bytes starting at
  //         the begining of the buffer, since it must be empty if there are
  //         still bytes due to step 1 either cleaned out the buffer or didn't
  //         have any more data to write.
  if (data.size_bytes() > 0) {
    PW_DCHECK(WriteBufferEmpty());
    std::memcpy(write_buffer_.data(), data.data(), data.size_bytes());
    write_address_ += data.size_bytes();
  }

  return Status::OK;
}

Status BlobStore::AddToWriteBuffer(ConstByteSpan data) {
  if (!ValidToWrite()) {
    return Status::DATA_LOSS;
  }
  if (WriteBytesRemaining() == 0) {
    return Status::OUT_OF_RANGE;
  }
  if (WriteBufferBytesFree() < data.size_bytes()) {
    return Status::RESOURCE_EXHAUSTED;
  }

  size_t bytes_in_buffer = WriteBufferBytesUsed();

  std::memcpy(
      write_buffer_.data() + bytes_in_buffer, data.data(), data.size_bytes());
  write_address_ += data.size_bytes();

  return Status::OK;
}

Status BlobStore::Flush() {
  if (!ValidToWrite()) {
    return Status::DATA_LOSS;
  }
  if (WriteBufferBytesUsed() == 0) {
    return Status::OK;
  }
  // Don't need to check available space, AddToWriteBuffer() will not enqueue
  // more than can be written to flash.

  Status status = EraseIfNeeded();
  // TODO: switch to TRY once available.
  if (!status.ok()) {
    return Status::DATA_LOSS;
  }

  ByteSpan data = std::span(write_buffer_.data(), WriteBufferBytesUsed());
  while (data.size_bytes() >= flash_write_size_bytes_) {
    Status status = CommitToFlash(data.first(flash_write_size_bytes_));
    // TODO: switch to TRY once available.
    if (!status.ok()) {
      return Status::DATA_LOSS;
    }

    data = data.subspan(flash_write_size_bytes_);
  }

  // Only a multiple of flash_write_size_bytes_ are written in the flush. Any
  // remainder is held until later for either a flush with
  // flash_write_size_bytes buffered or the writer is closed.
  if (!WriteBufferEmpty()) {
    PW_DCHECK_UINT_EQ(data.size_bytes(), WriteBufferBytesUsed());
    // For any leftover bytes less than the flash write size, move them to the
    // start of the bufer.
    std::memmove(write_buffer_.data(), data.data(), data.size_bytes());
  } else {
    PW_DCHECK_UINT_EQ(data.size_bytes(), 0);
  }

  return Status::OK;
}

Status BlobStore::FlushFinalPartialChunk() {
  size_t bytes_in_buffer = WriteBufferBytesUsed();

  PW_DCHECK_UINT_GT(bytes_in_buffer, 0);
  PW_DCHECK_UINT_LE(bytes_in_buffer, flash_write_size_bytes_);
  PW_DCHECK_UINT_LE(flash_write_size_bytes_, WriteBytesRemaining());

  PW_LOG_DEBUG(
      "  Remainder %u bytes in write buffer to zero-pad to flash write "
      "size and commit",
      static_cast<unsigned>(bytes_in_buffer));

  // Zero out the remainder of the buffer.
  auto zero_span = write_buffer_.subspan(bytes_in_buffer);
  std::memset(zero_span.data(), 0, zero_span.size_bytes());
  // TODO: look in to using flash erased value for fill, to possibly allow
  // better resuming of writing.

  ConstByteSpan remaining_bytes = write_buffer_.first(flash_write_size_bytes_);
  return CommitToFlash(remaining_bytes, bytes_in_buffer);
}

Status BlobStore::CommitToFlash(ConstByteSpan source, size_t data_bytes) {
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

  if (!result.status().ok()) {
    valid_data_ = false;
  }

  return result.status();
}

// Needs to be in .cc file since PW_CHECK doesn't like being in .h files.
size_t BlobStore::WriteBufferBytesUsed() const {
  PW_CHECK_UINT_GE(write_address_, flash_address_);
  return write_address_ - flash_address_;
}

// Needs to be in .cc file since PW_DCHECK doesn't like being in .h files.
size_t BlobStore::WriteBufferBytesFree() const {
  PW_DCHECK_UINT_GE(write_buffer_.size_bytes(), WriteBufferBytesUsed());
  size_t buffer_remaining = write_buffer_.size_bytes() - WriteBufferBytesUsed();
  return std::min(buffer_remaining, WriteBytesRemaining());
}

Status BlobStore::EraseIfNeeded() {
  if (flash_address_ == 0) {
    // Always just erase. Erase is smart enough to only erase if needed.
    return Erase();
  }
  return Status::OK;
}

StatusWithSize BlobStore::Read(size_t offset, ByteSpan dest) {
  (void)offset;
  (void)dest;
  return StatusWithSize::UNIMPLEMENTED;
}

Result<ByteSpan> BlobStore::GetMemoryMappedBlob() const {
  if (flash_address_ == 0 || !valid_data_) {
    return Status::FAILED_PRECONDITION;
  }

  std::byte* mcu_address = partition_.PartitionAddressToMcuAddress(0);
  if (mcu_address == nullptr) {
    return Status::UNIMPLEMENTED;
  }
  return std::span(mcu_address, ReadableDataBytes());
}

size_t BlobStore::ReadableDataBytes() const { return flash_address_; }

Status BlobStore::Erase() {
  // Can't erase if any readers are open, since this would erase flash right out
  // from under them.
  if (readers_open_ != 0) {
    return Status::UNAVAILABLE;
  }

  // If already erased our work here is done.
  if (flash_erased_) {
    // The write buffer might have bytes when the erase happens.
    PW_DCHECK_UINT_LE(write_address_, write_buffer_.size_bytes());
    PW_DCHECK_UINT_EQ(flash_address_, 0);
    PW_DCHECK(valid_data_);
    return Status::OK;
  }

  Invalidate();

  Status status = partition_.Erase();

  if (status.ok()) {
    flash_erased_ = true;
    valid_data_ = true;
  }
  return status;
}

Status BlobStore::Invalidate() {
  if (readers_open_ != 0) {
    return Status::UNAVAILABLE;
  }

  Status status = kvs_.Delete(MetadataKey());
  valid_data_ = false;
  ResetChecksum();
  write_address_ = 0;
  flash_address_ = 0;

  return status;
}

Status BlobStore::ValidateChecksum() {
  if (!metadata_.complete) {
    return Status::UNAVAILABLE;
  }

  if (checksum_algo_ == nullptr) {
    if (metadata_.checksum != 0) {
      return Status::DATA_LOSS;
    }

    return Status::OK;
  }

  PW_LOG_DEBUG("Validate checksum of 0x%08x in flash for blob of %u bytes",
               static_cast<unsigned>(metadata_.checksum),
               static_cast<unsigned>(metadata_.data_size_bytes));
  auto result = CalculateChecksumFromFlash(metadata_.data_size_bytes);
  if (!result.ok()) {
    return result.status();
  }

  return checksum_algo_->Verify(as_bytes(std::span(&metadata_.checksum, 1)));
}

Result<BlobStore::ChecksumValue> BlobStore::CalculateChecksumFromFlash(
    size_t bytes_to_check) {
  if (checksum_algo_ == nullptr) {
    return Status::OK;
  }

  checksum_algo_->Reset();

  kvs::FlashPartition::Address address = 0;
  const kvs::FlashPartition::Address end = bytes_to_check;

  constexpr size_t kReadBufferSizeBytes = 32;
  std::array<std::byte, kReadBufferSizeBytes> buffer;
  while (address < end) {
    const size_t read_size = std::min(size_t(end - address), buffer.size());
    StatusWithSize status =
        partition_.Read(address, std::span(buffer).first(read_size));
    // TODO: switch to TRY once available.
    if (!status.ok()) {
      return status.status();
    }

    checksum_algo_->Update(buffer.data(), read_size);
    address += read_size;
  }

  ChecksumValue checksum_value;
  std::span checksum = checksum_algo_->Finish();
  std::memcpy(&checksum_value,
              checksum.data(),
              std::min(checksum.size(), sizeof(checksum_value)));
  return checksum_value;
}

}  // namespace pw::blob_store
