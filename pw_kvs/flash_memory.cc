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

#include "pw_kvs/flash_memory.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {

using std::byte;

Status FlashPartition::Erase(Address address, size_t num_sectors) {
  if (permission_ == PartitionPermission::kReadOnly) {
    return Status::PERMISSION_DENIED;
  }

  TRY(CheckBounds(address, num_sectors * sector_size_bytes()));
  return flash_.Erase(PartitionToFlashAddress(address), num_sectors);
}

StatusWithSize FlashPartition::Read(Address address, span<byte> output) {
  TRY(CheckBounds(address, output.size()));
  return flash_.Read(PartitionToFlashAddress(address), output);
}

StatusWithSize FlashPartition::Write(Address address, span<const byte> data) {
  if (permission_ == PartitionPermission::kReadOnly) {
    return Status::PERMISSION_DENIED;
  }
  TRY(CheckBounds(address, data.size()));
  return flash_.Write(PartitionToFlashAddress(address), data);
}

StatusWithSize FlashPartition::WriteAligned(
    const Address start_address, std::initializer_list<span<const byte>> data) {
  byte buffer[64];  // TODO: Configure this?

  Address address = start_address;
  auto bytes_written = [&]() { return address - start_address; };

  const size_t write_size = AlignDown(sizeof(buffer), alignment_bytes());
  size_t bytes_in_buffer = 0;

  for (span<const byte> chunk : data) {
    while (!chunk.empty()) {
      const size_t to_copy =
          std::min(write_size - bytes_in_buffer, chunk.size());

      std::memcpy(&buffer[bytes_in_buffer], chunk.data(), to_copy);
      chunk = chunk.subspan(to_copy);
      bytes_in_buffer += to_copy;

      // If the buffer is full, write it out.
      if (bytes_in_buffer == write_size) {
        Status status = Write(address, span(buffer, write_size));
        if (!status.ok()) {
          return StatusWithSize(status, bytes_written());
        }

        address += write_size;
        bytes_in_buffer = 0;
      }
    }
  }

  // If data remains in the buffer, pad it to the alignment size and flush
  // the remaining data.
  if (bytes_in_buffer != 0u) {
    size_t remaining_write_size = AlignUp(bytes_in_buffer, alignment_bytes());
    std::memset(
        &buffer[bytes_in_buffer], 0, remaining_write_size - bytes_in_buffer);
    if (Status status = Write(address, span(buffer, remaining_write_size));
        !status.ok()) {
      return StatusWithSize(status, bytes_written());
    }
    address += remaining_write_size;  // Include padding bytes in the total.
  }

  return StatusWithSize(bytes_written());
}

Status FlashPartition::IsRegionErased(Address source_flash_address,
                                      size_t length,
                                      bool* is_erased) {
  // Max alignment is artificial to keep the stack usage low for this
  // function. Using 16 because it's the alignment of encrypted flash.
  constexpr size_t kMaxAlignment = 16;

  // Relying on Read() to check address and len arguments.
  if (is_erased == nullptr) {
    return Status::INVALID_ARGUMENT;
  }
  const size_t alignment = alignment_bytes();
  if (alignment > kMaxAlignment || kMaxAlignment % alignment ||
      length % alignment) {
    return Status::INVALID_ARGUMENT;
  }

  byte buffer[kMaxAlignment];
  byte erased_pattern_buffer[kMaxAlignment];

  size_t offset = 0;
  std::memset(erased_pattern_buffer,
              int(flash_.erased_memory_content()),
              sizeof(erased_pattern_buffer));
  *is_erased = false;
  while (length > 0u) {
    // Check earlier that length is aligned, no need to round up
    size_t read_size = std::min(sizeof(buffer), length);
    TRY(Read(source_flash_address + offset, read_size, buffer).status());
    if (std::memcmp(buffer, erased_pattern_buffer, read_size)) {
      // Detected memory chunk is not entirely erased
      return Status::OK;
    }
    offset += read_size;
    length -= read_size;
  }
  *is_erased = true;
  return Status::OK;
}

Status FlashPartition::CheckBounds(Address address, size_t length) const {
  if (address + length > size_bytes()) {
    PW_LOG_ERROR("Attempted out-of-bound flash memory access (address: %" PRIu32
                 " length: %zu)",
                 address,
                 length);
    return Status::OUT_OF_RANGE;
  }
  return Status::OK;
}

}  // namespace pw::kvs
