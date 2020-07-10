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

#define PW_LOG_MODULE_NAME "KVS"

#include "pw_kvs/flash_memory.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "pw_kvs_private/config.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"
#include "pw_status/status_with_size.h"

namespace pw::kvs {

using std::byte;

StatusWithSize FlashPartition::Output::DoWrite(std::span<const byte> data) {
  TRY_WITH_SIZE(flash_.Write(address_, data));
  address_ += data.size();
  return StatusWithSize(data.size());
}

StatusWithSize FlashPartition::Input::DoRead(std::span<byte> data) {
  StatusWithSize result = flash_.Read(address_, data);
  address_ += result.size();
  return result;
}

Status FlashPartition::Erase(Address address, size_t num_sectors) {
  if (permission_ == PartitionPermission::kReadOnly) {
    return Status::PERMISSION_DENIED;
  }

  TRY(CheckBounds(address, num_sectors * sector_size_bytes()));
  return flash_.Erase(PartitionToFlashAddress(address), num_sectors);
}

StatusWithSize FlashPartition::Read(Address address, std::span<byte> output) {
  TRY_WITH_SIZE(CheckBounds(address, output.size()));
  return flash_.Read(PartitionToFlashAddress(address), output);
}

StatusWithSize FlashPartition::Write(Address address,
                                     std::span<const byte> data) {
  if (permission_ == PartitionPermission::kReadOnly) {
    return StatusWithSize::PERMISSION_DENIED;
  }
  TRY_WITH_SIZE(CheckBounds(address, data.size()));
  return flash_.Write(PartitionToFlashAddress(address), data);
}

Status FlashPartition::IsRegionErased(Address source_flash_address,
                                      size_t length,
                                      bool* is_erased) {
  // Relying on Read() to check address and len arguments.
  if (is_erased == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  // TODO(pwbug/214): Currently using a single flash alignment to do both the
  // read and write. The allowable flash read length may be less than what write
  // needs (possibly by a bunch), resulting in buffer and erased_pattern_buffer
  // being bigger than they need to be.
  const size_t alignment = alignment_bytes();
  if (alignment > kMaxFlashAlignment || kMaxFlashAlignment % alignment ||
      length % alignment) {
    return Status::INVALID_ARGUMENT;
  }

  byte buffer[kMaxFlashAlignment];
  const byte erased_byte = flash_.erased_memory_content();
  size_t offset = 0;
  *is_erased = false;
  while (length > 0u) {
    // Check earlier that length is aligned, no need to round up
    size_t read_size = std::min(sizeof(buffer), length);
    TRY(Read(source_flash_address + offset, read_size, buffer).status());

    for (byte b : std::span(buffer, read_size)) {
      if (b != erased_byte) {
        // Detected memory chunk is not entirely erased
        return Status::OK;
      }
    }

    offset += read_size;
    length -= read_size;
  }
  *is_erased = true;
  return Status::OK;
}

bool FlashPartition::AppearsErased(std::span<const byte> data) const {
  byte erased_content = flash_.erased_memory_content();
  for (byte b : data) {
    if (b != erased_content) {
      return false;
    }
  }
  return true;
}

Status FlashPartition::CheckBounds(Address address, size_t length) const {
  if (address + length > size_bytes()) {
    PW_LOG_ERROR(
        "Attempted out-of-bound flash memory access (address: %u length: %u)",
        unsigned(address),
        unsigned(length));
    return Status::OUT_OF_RANGE;
  }
  return Status::OK;
}

}  // namespace pw::kvs
