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

#include <array>

#include "pw_kvs/devices/flash_memory.h"
#include "pw_kvs/status.h"
#include "pw_kvs/status_macros.h"

namespace pw {

// This creates a buffer which mimics the behaviour of flash (requires erase,
// before write, checks alignments, and is addressed in sectors).
template <uint32_t kSectorSize, uint16_t kSectorCount>
class InMemoryFakeFlash : public FlashMemory {
 public:
  InMemoryFakeFlash(uint8_t alignment = 1)  // default 8 bit alignment
      : FlashMemory(kSectorSize, kSectorCount, alignment) {}

  // Always enabled
  Status Enable() override { return Status::OK; }
  Status Disable() override { return Status::OK; }
  bool IsEnabled() const override { return true; }

  // Erase num_sectors starting at a given address. Blocking call.
  // Address should be on a sector boundary.
  // Returns: OK, on success.
  //          INVALID_ARGUMENT, if address or sector count is invalid.
  //          UNKNOWN, on HAL error
  Status Erase(Address addr, uint32_t num_sectors) override {
    RETURN_STATUS_IF(addr % GetSectorSizeBytes() != 0,
                     Status::INVALID_ARGUMENT);
    RETURN_STATUS_IF(
        addr / GetSectorSizeBytes() + num_sectors > GetSectorCount(),
        Status::UNKNOWN);
    RETURN_STATUS_IF(addr % GetAlignmentBytes() != 0, Status::INVALID_ARGUMENT);
    memset(&buffer_[addr], 0xFF, GetSectorSizeBytes() * num_sectors);
    return Status::OK;
  }

  // Reads bytes from flash into buffer. Blocking call.
  // Returns: OK, on success.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  Status Read(uint8_t* dest_ram_addr,
              Address source_flash_addr,
              uint32_t len) override {
    RETURN_STATUS_IF(
        (source_flash_addr + len) >= GetSectorCount() * GetSizeBytes(),
        Status::INVALID_ARGUMENT);
    memcpy(dest_ram_addr, &buffer_[source_flash_addr], len);
    return Status::OK;
  }

  // Writes bytes to flash. Blocking call.
  // Returns: OK, on success.
  //          INVALID_ARGUMENT, if address or length is invalid.
  //          UNKNOWN, on HAL error
  Status Write(Address dest_flash_addr,
               const uint8_t* source_ram_addr,
               uint32_t len) override {
    RETURN_STATUS_IF(
        (dest_flash_addr + len) >= GetSectorCount() * GetSizeBytes(),
        Status::INVALID_ARGUMENT);
    RETURN_STATUS_IF(dest_flash_addr % GetAlignmentBytes() != 0,
                     Status::INVALID_ARGUMENT);
    RETURN_STATUS_IF(len % GetAlignmentBytes() != 0, Status::INVALID_ARGUMENT);
    // Check in erased state
    for (unsigned i = 0; i < len; i++) {
      RETURN_STATUS_IF(buffer_[dest_flash_addr + i] != 0xFF, Status::UNKNOWN);
    }
    memcpy(&buffer_[dest_flash_addr], source_ram_addr, len);
    return Status::OK;
  }

 private:
  std::array<uint8_t, kSectorCount * kSectorSize> buffer_;
};

}  // namespace pw
