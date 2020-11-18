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
    return Status::Ok();
  }

  PW_LOG_INFO("Init BlobStore");

  const size_t write_buffer_size_alignment =
      flash_write_size_bytes_ % partition_.alignment_bytes();
  PW_CHECK_UINT_EQ((write_buffer_size_alignment), 0);
  PW_CHECK_UINT_GE(write_buffer_.size_bytes(), flash_write_size_bytes_);
  PW_CHECK_UINT_GE(flash_write_size_bytes_, partition_.alignment_bytes());

  ResetChecksum();
  initialized_ = true;

  if (LoadMetadata().ok()) {
    valid_data_ = true;
    write_address_ = metadata_.data_size_bytes;
    flash_address_ = metadata_.data_size_bytes;

    PW_LOG_DEBUG("BlobStore init - Have valid blob of %u bytes",
                 static_cast<unsigned>(write_address_));
    return Status::Ok();
  }

  // No saved blob, check for flash being erased.
  bool erased = false;
  if (partition_.IsErased(&erased).ok() && erased) {
    flash_erased_ = true;

    // Blob data is considered valid as soon as the flash is erased. Even though
    // there are 0 bytes written, they are valid.
    valid_data_ = true;
    PW_LOG_DEBUG("BlobStore init - is erased");
  } else {
    PW_LOG_DEBUG("BlobStore init - not erased");
  }
  return Status::Ok();
}

Status BlobStore::LoadMetadata() {
  if (!kvs_.Get(MetadataKey(), &metadata_).ok()) {
    // If no metadata was read, make sure the metadata is reset.
    metadata_.reset();
    return Status::NotFound();
  }

  if (!ValidateChecksum().ok()) {
    PW_LOG_ERROR("BlobStore init - Invalidating blob with invalid checksum");
    Invalidate();
    return Status::DataLoss();
  }

  return Status::Ok();
}

size_t BlobStore::MaxDataSizeBytes() const { return partition_.size_bytes(); }

Status BlobStore::OpenWrite() {
  if (!initialized_) {
    return Status::FailedPrecondition();
  }

  // Writer can only be opened if there are no other writer or readers already
  // open.
  if (writer_open_ || readers_open_ != 0) {
    return Status::Unavailable();
  }

  PW_LOG_DEBUG("Blob writer open");

  writer_open_ = true;

  Invalidate();

  return Status::Ok();
}

Status BlobStore::OpenRead() {
  if (!initialized_) {
    return Status::FailedPrecondition();
  }

  // Reader can only be opened if there is no writer open.
  if (writer_open_) {
    return Status::Unavailable();
  }

  if (!ValidToRead()) {
    PW_LOG_ERROR("Blob reader unable open without valid data");
    return Status::FailedPrecondition();
  }

  PW_LOG_DEBUG("Blob reader open");

  readers_open_++;
  return Status::Ok();
}

Status BlobStore::CloseWrite() {
  auto do_close_write = [&]() -> Status {
    // If not valid to write, there was data loss and the close will result in a
    // not valid blob. Don't need to flush any write buffered bytes.
    if (!ValidToWrite()) {
      return Status::DataLoss();
    }

    if (write_address_ == 0) {
      return Status::Ok();
    }

    PW_LOG_DEBUG(
        "Blob writer close of %u byte blob, with %u bytes still in write "
        "buffer",
        static_cast<unsigned>(write_address_),
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
    PW_DCHECK(WriteBufferEmpty());

    // If things are still good, save the blob metadata.
    metadata_ = {.checksum = 0, .data_size_bytes = flash_address_};
    if (checksum_algo_ != nullptr) {
      ConstByteSpan checksum = checksum_algo_->Finish();
      std::memcpy(&metadata_.checksum,
                  checksum.data(),
                  std::min(checksum.size(), sizeof(metadata_.checksum)));
    }

    if (!ValidateChecksum().ok()) {
      Invalidate();
      return Status::DataLoss();
    }

    if (!kvs_.Put(MetadataKey(), metadata_).ok()) {
      return Status::DataLoss();
    }

    return Status::Ok();
  };

  const Status status = do_close_write();
  writer_open_ = false;

  if (!status.ok()) {
    valid_data_ = false;
    return Status::DataLoss();
  }
  return Status::Ok();
}

Status BlobStore::CloseRead() {
  PW_CHECK_UINT_GT(readers_open_, 0);
  readers_open_--;
  PW_LOG_DEBUG("Blob reader close");
  return Status::Ok();
}

Status BlobStore::Write(ConstByteSpan data) {
  if (!ValidToWrite()) {
    return Status::DataLoss();
  }
  if (data.size_bytes() == 0) {
    return Status::Ok();
  }
  if (WriteBytesRemaining() == 0) {
    return Status::OutOfRange();
  }
  if (WriteBytesRemaining() < data.size_bytes()) {
    return Status::ResourceExhausted();
  }

  if (!EraseIfNeeded().ok()) {
    return Status::DataLoss();
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
      return Status::Ok();
    }

    // The write buffer is full, flush to flash.
    if (!CommitToFlash(write_buffer_).ok()) {
      return Status::DataLoss();
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
    if (!CommitToFlash(data.first(flash_write_size_bytes_)).ok()) {
      return Status::DataLoss();
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

  return Status::Ok();
}

Status BlobStore::AddToWriteBuffer(ConstByteSpan data) {
  if (!ValidToWrite()) {
    return Status::DataLoss();
  }
  if (WriteBytesRemaining() == 0) {
    return Status::OutOfRange();
  }
  if (WriteBufferBytesFree() < data.size_bytes()) {
    return Status::ResourceExhausted();
  }

  size_t bytes_in_buffer = WriteBufferBytesUsed();

  std::memcpy(
      write_buffer_.data() + bytes_in_buffer, data.data(), data.size_bytes());
  write_address_ += data.size_bytes();

  return Status::Ok();
}

Status BlobStore::Flush() {
  if (!ValidToWrite()) {
    return Status::DataLoss();
  }
  if (WriteBufferBytesUsed() == 0) {
    return Status::Ok();
  }
  // Don't need to check available space, AddToWriteBuffer() will not enqueue
  // more than can be written to flash.

  if (!EraseIfNeeded().ok()) {
    return Status::DataLoss();
  }

  ByteSpan data = std::span(write_buffer_.data(), WriteBufferBytesUsed());
  while (data.size_bytes() >= flash_write_size_bytes_) {
    if (!CommitToFlash(data.first(flash_write_size_bytes_)).ok()) {
      return Status::DataLoss();
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

  return Status::Ok();
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
  std::memset(zero_span.data(),
              static_cast<int>(partition_.erased_memory_content()),
              zero_span.size_bytes());

  ConstByteSpan remaining_bytes = write_buffer_.first(flash_write_size_bytes_);
  return CommitToFlash(remaining_bytes, bytes_in_buffer);
}

Status BlobStore::CommitToFlash(ConstByteSpan source, size_t data_bytes) {
  if (data_bytes == 0) {
    data_bytes = source.size_bytes();
  }
  flash_erased_ = false;
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
  return Status::Ok();
}

StatusWithSize BlobStore::Read(size_t offset, ByteSpan dest) const {
  if (!ValidToRead()) {
    return StatusWithSize::FailedPrecondition();
  }
  if (offset >= ReadableDataBytes()) {
    return StatusWithSize::OutOfRange();
  }

  size_t available_bytes = ReadableDataBytes() - offset;
  size_t read_size = std::min(available_bytes, dest.size_bytes());

  return partition_.Read(offset, dest.first(read_size));
}

Result<ConstByteSpan> BlobStore::GetMemoryMappedBlob() const {
  if (!ValidToRead()) {
    return Status::FailedPrecondition();
  }

  std::byte* mcu_address = partition_.PartitionAddressToMcuAddress(0);
  if (mcu_address == nullptr) {
    return Status::Unimplemented();
  }
  return ConstByteSpan(mcu_address, ReadableDataBytes());
}

size_t BlobStore::ReadableDataBytes() const {
  // TODO: clean up state related to readable bytes.
  return flash_address_;
}

Status BlobStore::Erase() {
  // If already erased our work here is done.
  if (flash_erased_) {
    // The write buffer might already have bytes when this call happens, due to
    // a deferred write.
    PW_DCHECK_UINT_LE(write_address_, write_buffer_.size_bytes());
    PW_DCHECK_UINT_EQ(flash_address_, 0);

    // Erased blobs should be valid as soon as the flash is erased. Even though
    // there are 0 bytes written, they are valid.
    PW_DCHECK(valid_data_);
    return Status::Ok();
  }

  Invalidate();

  Status status = partition_.Erase();

  if (status.ok()) {
    flash_erased_ = true;

    // Blob data is considered valid as soon as the flash is erased. Even though
    // there are 0 bytes written, they are valid.
    valid_data_ = true;
  }
  return status;
}

Status BlobStore::Invalidate() {
  metadata_.reset();

  // Blob data is considered if the flash is erased. Even though
  // there are 0 bytes written, they are valid.
  valid_data_ = flash_erased_;
  ResetChecksum();
  write_address_ = 0;
  flash_address_ = 0;

  Status status = kvs_.Delete(MetadataKey());

  return (status == Status::Ok() || status == Status::NotFound())
             ? Status::Ok()
             : Status::Internal();
}

Status BlobStore::ValidateChecksum() {
  if (metadata_.data_size_bytes == 0) {
    PW_LOG_INFO("Blob unable to validate checksum of an empty blob");
    return Status::Unavailable();
  }

  if (checksum_algo_ == nullptr) {
    if (metadata_.checksum != 0) {
      PW_LOG_ERROR(
          "Blob invalid to have a checkum value with no checksum algo");
      return Status::DataLoss();
    }

    return Status::Ok();
  }

  PW_LOG_DEBUG("Validate checksum of 0x%08x in flash for blob of %u bytes",
               static_cast<unsigned>(metadata_.checksum),
               static_cast<unsigned>(metadata_.data_size_bytes));
  PW_TRY(CalculateChecksumFromFlash(metadata_.data_size_bytes));

  Status status =
      checksum_algo_->Verify(as_bytes(std::span(&metadata_.checksum, 1)));
  PW_LOG_DEBUG("  checksum verify of %s", status.str());

  return status;
}

Status BlobStore::CalculateChecksumFromFlash(size_t bytes_to_check) {
  if (checksum_algo_ == nullptr) {
    return Status::Ok();
  }

  checksum_algo_->Reset();

  kvs::FlashPartition::Address address = 0;
  const kvs::FlashPartition::Address end = bytes_to_check;

  constexpr size_t kReadBufferSizeBytes = 32;
  std::array<std::byte, kReadBufferSizeBytes> buffer;
  while (address < end) {
    const size_t read_size = std::min(size_t(end - address), buffer.size());
    PW_TRY(partition_.Read(address, std::span(buffer).first(read_size)));

    checksum_algo_->Update(buffer.data(), read_size);
    address += read_size;
  }

  // Safe to ignore the return from Finish, checksum_algo_ keeps the state
  // information that it needs.
  checksum_algo_->Finish();
  return Status::Ok();
}

}  // namespace pw::blob_store
