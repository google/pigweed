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

  bool erased = false;
  if (partition_.IsErased(&erased).ok() && erased) {
    flash_erased_ = true;
  } else {
    flash_erased_ = false;
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
  PW_DCHECK(writer_open_);
  Status status = Status::OK;

  // Only need to finish a blob if the blob has any bytes.
  if (write_address_ > 0) {
    // Flush bytes in buffer.
    if (write_address_ != flash_address_) {
      PW_CHECK_UINT_GE(write_address_, flash_address_);
      size_t bytes_in_buffer = write_address_ - flash_address_;

      // Zero out the remainder of the buffer.
      auto zero_span = write_buffer_.subspan(bytes_in_buffer);
      std::memset(zero_span.data(), 0, zero_span.size_bytes());

      status = CommitToFlash(write_buffer_, bytes_in_buffer);
    }

    if (status.ok()) {
      // Buffer should be clear unless there was a write error.
      PW_DCHECK_UINT_EQ(flash_address_, write_address_);
      // Should not be completing a blob write when it has completed metadata.
      PW_DCHECK(!metadata_.complete);

      metadata_ = {
          .checksum = 0, .data_size_bytes = flash_address_, .complete = true};

      if (checksum_algo_ != nullptr) {
        ConstByteSpan checksum = checksum_algo_->Finish();
        std::memcpy(&metadata_.checksum,
                    checksum.data(),
                    std::min(checksum.size(), sizeof(metadata_.checksum)));
      }

      status = kvs_.Put(MetadataKey(), metadata_);
    }
  }
  writer_open_ = false;

  if (status.ok()) {
    return ValidateChecksum();
  }
  return Status::DATA_LOSS;
}

Status BlobStore::CloseRead() {
  PW_CHECK_UINT_GT(readers_open_, 0);
  readers_open_--;
  return Status::OK;
}

Status BlobStore::Write(ConstByteSpan data) {
  PW_DCHECK(writer_open_);

  if (data.size_bytes() == 0) {
    return Status::OK;
  }
  if (WriteBytesRemaining() == 0) {
    return Status::OUT_OF_RANGE;
  }
  if (WriteBytesRemaining() < data.size_bytes()) {
    return Status::RESOURCE_EXHAUSTED;
  }

  if (write_address_ == 0 && !erased()) {
    Erase();
  }

  // Write in (up to) 3 steps:
  // 1) Finish filling write buffer and if full write it to flash.
  // 2) Write as many whole block-sized chunks as the data has remaining
  //    after 1.
  // 3) Put any remaining bytes less than flash write size in the write buffer.

  // Step 1) If there is any data in the write buffer, finish filling write
  //         buffer and if full write it to flash.
  const size_t write_chunk_bytes = write_buffer_.size_bytes();
  if (!WriteBufferEmpty()) {
    PW_CHECK_UINT_GE(write_address_, flash_address_);
    size_t bytes_in_buffer = write_address_ - flash_address_;
    PW_CHECK_UINT_GE(write_chunk_bytes, bytes_in_buffer);

    size_t buffer_remaining = write_chunk_bytes - bytes_in_buffer;

    // Add bytes up to filling the write buffer.
    size_t add_bytes = std::min(buffer_remaining, data.size_bytes());
    std::memcpy(write_buffer_.data() + bytes_in_buffer, data.data(), add_bytes);
    write_address_ += add_bytes;
    bytes_in_buffer += add_bytes;
    data = data.subspan(add_bytes);

    if (bytes_in_buffer != write_chunk_bytes) {
      // If there was not enough bytes to finish filling the write buffer, there
      // should not be any bytes left.
      PW_DCHECK(data.size_bytes() == 0);
      return Status::OK;
    }

    // The write buffer is full, flush to flash.
    Status status = CommitToFlash(write_buffer_);
    if (!status.ok()) {
      return status;
    }

    PW_DCHECK(WriteBufferEmpty());
  }

  // At this point, if data.size_bytes() > 0, the write buffer should be empty.
  // This invariant is checked as part of of steps 2 & 3.

  // Step 2) Write as many block-sized chunks as the data has remaining after
  //         step 1.
  // TODO: Decouple write buffer size from optimal bulk write size.
  while (data.size_bytes() >= write_chunk_bytes) {
    PW_DCHECK(WriteBufferEmpty());

    write_address_ += write_chunk_bytes;
    Status status = CommitToFlash(data.first(write_chunk_bytes));
    if (!status.ok()) {
      return status;
    }

    data = data.subspan(write_chunk_bytes);
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

StatusWithSize BlobStore::Read(size_t offset, ByteSpan dest) {
  (void)offset;
  (void)dest;
  return StatusWithSize::UNIMPLEMENTED;
}

Result<ByteSpan> BlobStore::GetMemoryMappedBlob() const {
  if (write_address_ == 0 || !valid_data_) {
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
  PW_DCHECK(writer_open_);

  // Can't erase if any readers are open, since this would erase flash right out
  // from under them.
  if (readers_open_ != 0) {
    return Status::UNAVAILABLE;
  }

  // If already erased our work here is done.
  if (flash_erased_) {
    PW_DCHECK_UINT_EQ(write_address_, 0);
    PW_DCHECK_UINT_EQ(flash_address_, 0);
    PW_DCHECK(!valid_data_);
    return Status::OK;
  }

  Invalidate();

  Status status = partition_.Erase();

  if (status.ok()) {
    flash_erased_ = true;
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
               unsigned(metadata_.checksum),
               unsigned(metadata_.data_size_bytes));
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
