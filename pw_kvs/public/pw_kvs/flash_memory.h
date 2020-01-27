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

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "pw_kvs/assert.h"
#include "pw_kvs/partition_table_entry.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::kvs {

class FlashMemory {
 public:
  // The flash address is in the range of: 0 to FlashSize.
  typedef uint32_t Address;
  constexpr FlashMemory(uint32_t sector_size,
                        uint32_t sector_count,
                        uint8_t alignment,
                        uint32_t start_address = 0,
                        uint32_t sector_start = 0,
                        uint8_t erased_memory_content = 0xFF)
      : sector_size_(sector_size),
        flash_sector_count_(sector_count),
        alignment_(alignment),
        start_address_(start_address),
        sector_start_(sector_start),
        erased_memory_content_(erased_memory_content) {}

  virtual ~FlashMemory() = default;

  virtual Status Enable() = 0;
  virtual Status Disable() = 0;
  virtual bool IsEnabled() const = 0;
  virtual Status SelfTest() { return Status::UNIMPLEMENTED; }

  // Erase num_sectors starting at a given address. Blocking call.
  // Address should be on a sector boundary.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or sector count is invalid.
  //          UNKNOWN, on HAL error
  virtual Status Erase(Address flash_address, uint32_t num_sectors) = 0;

  // Reads bytes from flash into buffer. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual Status Read(uint8_t* destination_ram_address,
                      Address source_flash_address,
                      uint32_t len) = 0;

  // Writes bytes to flash. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual Status Write(Address destination_flash_address,
                       const uint8_t* source_ram_address,
                       uint32_t len) = 0;

  // Convert an Address to an MCU pointer, this can be used for memory
  // mapped reads. Return NULL if the memory is not memory mapped.
  virtual uint8_t* FlashAddressToMcuAddress(Address) const { return nullptr; }

  // GetStartSector() is useful for FlashMemory instances where the
  // sector start is not 0. (ex.: cases where there are portions of flash
  // that should be handled independently).
  constexpr uint32_t GetStartSector() const { return sector_start_; }
  constexpr uint32_t GetSectorSizeBytes() const { return sector_size_; }
  constexpr uint32_t GetSectorCount() const { return flash_sector_count_; }
  constexpr uint8_t GetAlignmentBytes() const { return alignment_; }
  constexpr uint32_t GetSizeBytes() const {
    return sector_size_ * flash_sector_count_;
  }
  // Address of the start of flash (the address of sector 0)
  constexpr uint32_t GetStartAddress() const { return start_address_; }
  constexpr uint8_t GetErasedMemoryContent() const {
    return erased_memory_content_;
  }

 private:
  const uint32_t sector_size_;
  const uint32_t flash_sector_count_;
  const uint8_t alignment_;
  const uint32_t start_address_;
  const uint32_t sector_start_;
  const uint8_t erased_memory_content_;
};

class FlashPartition {
 public:
  // The flash address is in the range of: 0 to PartitionSize.
  typedef uint32_t Address;

  constexpr FlashPartition(
      FlashMemory* flash,
      uint32_t start_sector_index,
      uint32_t sector_count,
      PartitionPermission permission = PartitionPermission::kReadAndWrite)
      : flash_(*flash),
        start_sector_index_(start_sector_index),
        sector_count_(sector_count),
        permission_(permission) {}

  constexpr FlashPartition(FlashMemory* flash, PartitionTableEntry entry)
      : flash_(*flash),
        start_sector_index_(entry.partition_start_sector_index),
        sector_count_(entry.partition_end_sector_index -
                      entry.partition_start_sector_index + 1),
        permission_(entry.partition_permission) {}

  virtual ~FlashPartition() = default;

  // Erase num_sectors starting at a given address. Blocking call.
  // Address should be on a sector boundary.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or sector count is invalid.
  //          PERMISSION_DENIED, if partition is read only.
  //          UNKNOWN, on HAL error
  virtual Status Erase(Address address, uint32_t num_sectors) {
    if (permission_ == PartitionPermission::kReadOnly) {
      return Status::PERMISSION_DENIED;
    }
    if (Status status =
            CheckBounds(address, num_sectors * GetSectorSizeBytes());
        !status.ok()) {
      return status;
    }
    return flash_.Erase(PartitionToFlashAddress(address), num_sectors);
  }

  // Reads bytes from flash into buffer. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual Status Read(uint8_t* destination_ram_address,
                      Address source_flash_address,
                      uint32_t len) {
    if (Status status = CheckBounds(source_flash_address, len); !status.ok()) {
      return status;
    }
    return flash_.Read(destination_ram_address,
                       PartitionToFlashAddress(source_flash_address),
                       len);
  }

  // Writes bytes to flash. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          PERMISSION_DENIED, if partition is read only.
  //          UNKNOWN, on HAL error
  virtual Status Write(Address destination_flash_address,
                       const uint8_t* source_ram_address,
                       uint32_t len) {
    if (permission_ == PartitionPermission::kReadOnly) {
      return Status::PERMISSION_DENIED;
    }
    if (Status status = CheckBounds(destination_flash_address, len);
        !status.ok()) {
      return status;
    }
    return flash_.Write(PartitionToFlashAddress(destination_flash_address),
                        source_ram_address,
                        len);
  }

  // Check to see if chunk of flash memory is erased. Address and len need to
  // be aligned with FlashMemory.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual Status IsChunkErased(Address source_flash_address,
                               uint32_t len,
                               bool* is_erased) {
    // Max alignment is artifical to keep the stack usage low for this
    // function. Using 16 because it's the alignment of encrypted flash.
    const uint8_t kMaxAlignment = 16;
    // Relying on Read() to check address and len arguments.
    if (!is_erased) {
      return Status::INVALID_ARGUMENT;
    }
    uint8_t alignment = GetAlignmentBytes();
    if (alignment > kMaxAlignment || kMaxAlignment % alignment ||
        len % alignment) {
      return Status::INVALID_ARGUMENT;
    }

    uint8_t buffer[kMaxAlignment];
    uint8_t erased_pattern_buffer[kMaxAlignment];
    size_t offset = 0;
    std::memset(erased_pattern_buffer,
                flash_.GetErasedMemoryContent(),
                sizeof(erased_pattern_buffer));
    *is_erased = false;
    while (len > 0) {
      // Check earlier that len is aligned, no need to round up
      uint16_t read_size = std::min(static_cast<uint32_t>(sizeof(buffer)), len);
      if (Status status =
              Read(buffer, source_flash_address + offset, read_size);
          !status.ok()) {
        return status;
      }
      if (std::memcmp(buffer, erased_pattern_buffer, read_size)) {
        // Detected memory chunk is not entirely erased
        return Status::OK;
      }
      offset += read_size;
      len -= read_size;
    }
    *is_erased = true;
    return Status::OK;
  }

  constexpr uint32_t GetSectorSizeBytes() const {
    return flash_.GetSectorSizeBytes();
  }

  uint32_t GetSizeBytes() const {
    return GetSectorCount() * GetSectorSizeBytes();
  }

  virtual uint8_t GetAlignmentBytes() const {
    return flash_.GetAlignmentBytes();
  }

  virtual uint32_t GetSectorCount() const { return sector_count_; }

  // Convert a FlashMemory::Address to an MCU pointer, this can be used for
  // memory mapped reads. Return NULL if the memory is not memory mapped.
  uint8_t* PartitionAddressToMcuAddress(Address address) const {
    return flash_.FlashAddressToMcuAddress(PartitionToFlashAddress(address));
  }

  FlashMemory::Address PartitionToFlashAddress(Address address) const {
    return flash_.GetStartAddress() +
           (start_sector_index_ - flash_.GetStartSector()) *
               GetSectorSizeBytes() +
           address;
  }

 protected:
  Status CheckBounds(Address address, size_t len) const {
    if (address + len > GetSizeBytes()) {
      PW_LOG_ERROR(
          "Attempted out-of-bound flash memory access (address: %" PRIu32
          " length: %zu)",
          address,
          len);
      return Status::INVALID_ARGUMENT;
    }
    return Status::OK;
  }

 private:
  FlashMemory& flash_;
  const uint32_t start_sector_index_;
  const uint32_t sector_count_;
  const PartitionPermission permission_;
};

// FlashSubPartition defines a new partition which maps itself as a smaller
// piece of another partition. This can used when a partition has special
// behaviours (for example encrypted flash).
// For example, this will be the first sector of test_partition:
//    FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
class FlashSubPartition : public FlashPartition {
 public:
  constexpr FlashSubPartition(FlashPartition* parent_partition,
                              uint32_t start_sector_index,
                              uint32_t sector_count)
      : FlashPartition(*parent_partition),
        partition_(parent_partition),
        start_sector_index_(start_sector_index),
        sector_count_(sector_count) {}

  Status Erase(Address address, uint32_t num_sectors) override {
    if (Status status =
            CheckBounds(address, num_sectors * GetSectorSizeBytes());
        !status.ok()) {
      return status;
    }
    return partition_->Erase(ParentAddress(address), num_sectors);
  }

  Status Read(uint8_t* destination_ram_address,
              Address source_flash_address,
              uint32_t len) override {
    if (Status status = CheckBounds(source_flash_address, len); !status.ok()) {
      return status;
    }
    return partition_->Read(
        destination_ram_address, ParentAddress(source_flash_address), len);
  }

  Status Write(Address destination_flash_address,
               const uint8_t* source_ram_address,
               uint32_t len) override {
    if (Status status = CheckBounds(destination_flash_address, len);
        !status.ok()) {
      return status;
    }
    return partition_->Write(
        ParentAddress(destination_flash_address), source_ram_address, len);
  }

  Status IsChunkErased(Address source_flash_address,
                       uint32_t len,
                       bool* is_erased) override {
    if (Status status = CheckBounds(source_flash_address, len); !status.ok()) {
      return status;
    }
    return partition_->IsChunkErased(
        ParentAddress(source_flash_address), len, is_erased);
  }

  uint8_t GetAlignmentBytes() const override {
    return partition_->GetAlignmentBytes();
  }

  uint32_t GetSectorCount() const override { return sector_count_; }

 private:
  Address ParentAddress(Address address) const {
    return address + start_sector_index_ * partition_->GetSectorSizeBytes();
  }
  FlashPartition* partition_;
  const uint32_t start_sector_index_;
  const uint32_t sector_count_;
};

}  // namespace pw::kvs
