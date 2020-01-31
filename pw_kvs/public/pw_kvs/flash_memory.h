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
#include <cstdint>
#include <initializer_list>

#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {

// TODO: These are general-purpose utility functions that should be moved
//       elsewhere.
constexpr size_t AlignDown(size_t value, size_t alignment) {
  return (value / alignment) * alignment;
}

constexpr size_t AlignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace pw

namespace pw::kvs {

enum class PartitionPermission : bool {
  kReadOnly,
  kReadAndWrite,
};

class FlashMemory {
 public:
  // The flash address is in the range of: 0 to FlashSize.
  typedef uint32_t Address;
  constexpr FlashMemory(size_t sector_size,
                        size_t sector_count,
                        size_t alignment,
                        uint32_t start_address = 0,
                        uint32_t sector_start = 0,
                        std::byte erased_memory_content = std::byte{0xFF})
      : sector_size_(sector_size),
        flash_sector_count_(sector_count),
        alignment_(alignment),
        start_address_(start_address),
        start_sector_(sector_start),
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
  virtual Status Erase(Address flash_address, size_t num_sectors) = 0;

  // Reads bytes from flash into buffer. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual StatusWithSize Read(Address address, span<std::byte> output) = 0;

  // Writes bytes to flash. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual StatusWithSize Write(Address destination_flash_address,
                               span<const std::byte> data) = 0;

  // Convert an Address to an MCU pointer, this can be used for memory
  // mapped reads. Return NULL if the memory is not memory mapped.
  virtual std::byte* FlashAddressToMcuAddress(Address) const { return nullptr; }

  // start_sector() is useful for FlashMemory instances where the
  // sector start is not 0. (ex.: cases where there are portions of flash
  // that should be handled independently).
  constexpr uint32_t start_sector() const { return start_sector_; }
  constexpr size_t sector_size_bytes() const { return sector_size_; }
  constexpr size_t sector_count() const { return flash_sector_count_; }
  constexpr size_t alignment_bytes() const { return alignment_; }
  constexpr size_t size_bytes() const {
    return sector_size_ * flash_sector_count_;
  }
  // Address of the start of flash (the address of sector 0)
  constexpr uint32_t start_address() const { return start_address_; }
  constexpr std::byte erased_memory_content() const {
    return erased_memory_content_;
  }

 private:
  const uint32_t sector_size_;
  const uint32_t flash_sector_count_;
  const uint8_t alignment_;
  const uint32_t start_address_;
  const uint32_t start_sector_;
  const std::byte erased_memory_content_;
};

class FlashPartition {
 public:
  // The flash address is in the range of: 0 to PartitionSize.
  using Address = uint32_t;

  constexpr FlashPartition(
      FlashMemory* flash,
      uint32_t start_sector_index,
      uint32_t sector_count,
      PartitionPermission permission = PartitionPermission::kReadAndWrite)
      : flash_(*flash),
        start_sector_index_(start_sector_index),
        sector_count_(sector_count),
        permission_(permission) {}

#if 0
  constexpr FlashPartition(
      FlashMemory* flash,
      uint32_t start_sector_index,
      uint32_t end_sector_index,
      PartitionPermission permission = PartitionPermission::kReadAndWrite)
      : flash_(*flash),
        start_sector_index_(start_sector_index),
        sector_count_(end_sector_index - start_sector_index + 1),
        permission_(permission) {}
#endif

  virtual ~FlashPartition() = default;

  // Erase num_sectors starting at a given address. Blocking call.
  // Address should be on a sector boundary.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or sector count is invalid.
  //          PERMISSION_DENIED, if partition is read only.
  //          UNKNOWN, on HAL error
  virtual Status Erase(Address address, size_t num_sectors);

  // Reads bytes from flash into buffer. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  virtual StatusWithSize Read(Address address, span<std::byte> output);

  StatusWithSize Read(Address address, size_t length, void* output) {
    return Read(address, span(static_cast<std::byte*>(output), length));
  }

  // Writes bytes to flash. Blocking call.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          PERMISSION_DENIED, if partition is read only.
  //          UNKNOWN, on HAL error
  virtual StatusWithSize Write(Address address, span<const std::byte> data);

  StatusWithSize Write(Address start_address,
                       std::initializer_list<span<const std::byte>> data);

  // Check to see if chunk of flash memory is erased. Address and len need to
  // be aligned with FlashMemory.
  // Returns: OK, on success.
  //          TIMEOUT, on timeout.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  // TODO: StatusWithBool
  virtual Status IsRegionErased(Address source_flash_address,
                                size_t len,
                                bool* is_erased);

  constexpr uint32_t sector_size_bytes() const {
    return flash_.sector_size_bytes();
  }

  // Overridden by base classes which store metadata at the start of a sector.
  virtual uint32_t sector_available_size_bytes() const {
    return sector_size_bytes();
  }

  size_t size_bytes() const { return sector_count() * sector_size_bytes(); }

  virtual size_t alignment_bytes() const { return flash_.alignment_bytes(); }

  virtual size_t sector_count() const { return sector_count_; }

  // Convert a FlashMemory::Address to an MCU pointer, this can be used for
  // memory mapped reads. Return NULL if the memory is not memory mapped.
  std::byte* PartitionAddressToMcuAddress(Address address) const {
    return flash_.FlashAddressToMcuAddress(PartitionToFlashAddress(address));
  }

  FlashMemory::Address PartitionToFlashAddress(Address address) const {
    return flash_.start_address() +
           (start_sector_index_ - flash_.start_sector()) * sector_size_bytes() +
           address;
  }

 protected:
  Status CheckBounds(Address address, size_t len) const;

 private:
  FlashMemory& flash_;
  const uint32_t start_sector_index_;
  const uint32_t sector_count_;
  const PartitionPermission permission_;
};

}  // namespace pw::kvs
