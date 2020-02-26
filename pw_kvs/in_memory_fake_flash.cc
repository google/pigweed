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

#include "pw_kvs/in_memory_fake_flash.h"

#include "pw_log/log.h"

namespace pw::kvs {

Status FlashError::Check(span<FlashError> errors,
                         FlashMemory::Address address,
                         size_t size) {
  for (auto& error : errors) {
    if (Status status = error.Check(address, size); !status.ok()) {
      return status;
    }
  }

  return Status::OK;
}

Status FlashError::Check(FlashMemory::Address start_address, size_t size) {
  // Check if the event overlaps with this address range.
  if (begin_ != kAnyAddress &&
      (start_address >= end_ || (start_address + size) < begin_)) {
    return Status::OK;
  }

  if (delay_ > 0u) {
    delay_ -= 1;
    return Status::OK;
  }

  if (remaining_ == 0u) {
    return Status::OK;
  }

  if (remaining_ != kAlways) {
    remaining_ -= 1;
  }

  return status_;
}

Status InMemoryFakeFlash::Erase(Address address, size_t num_sectors) {
  if (address % sector_size_bytes() != 0) {
    PW_LOG_ERROR(
        "Attempted to erase sector at non-sector aligned boundary; address %zx",
        size_t(address));
    return Status::INVALID_ARGUMENT;
  }
  const size_t sector_id = address / sector_size_bytes();
  if (address / sector_size_bytes() + num_sectors > sector_count()) {
    PW_LOG_ERROR(
        "Tried to erase a sector at an address past flash end; "
        "address: %zx, sector implied: %zu",
        size_t(address),
        sector_id);
    return Status::OUT_OF_RANGE;
  }

  std::memset(
      &buffer_[address], int(kErasedValue), sector_size_bytes() * num_sectors);
  return Status::OK;
}

StatusWithSize InMemoryFakeFlash::Read(Address address,
                                       span<std::byte> output) {
  if (address + output.size() >= sector_count() * size_bytes()) {
    return StatusWithSize::OUT_OF_RANGE;
  }

  // Check for injected read errors
  Status status = FlashError::Check(read_errors_, address, output.size());
  std::memcpy(output.data(), &buffer_[address], output.size());
  return StatusWithSize(status, output.size());
}

StatusWithSize InMemoryFakeFlash::Write(Address address,
                                        span<const std::byte> data) {
  if (address % alignment_bytes() != 0 ||
      data.size() % alignment_bytes() != 0) {
    PW_LOG_ERROR("Unaligned write; address %zx, size %zu B, alignment %zu",
                 size_t(address),
                 data.size(),
                 alignment_bytes());
    return StatusWithSize::INVALID_ARGUMENT;
  }

  if (data.size() > sector_size_bytes() - (address % sector_size_bytes())) {
    PW_LOG_ERROR("Write crosses sector boundary; address %zx, size %zu B",
                 size_t(address),
                 data.size());
    return StatusWithSize::INVALID_ARGUMENT;
  }

  if (address + data.size() > sector_count() * sector_size_bytes()) {
    PW_LOG_ERROR(
        "Write beyond end of memory; address %zx, size %zu B, max address %zx",
        size_t(address),
        data.size(),
        sector_count() * sector_size_bytes());
    return StatusWithSize::OUT_OF_RANGE;
  }

  // Check in erased state
  for (unsigned i = 0; i < data.size(); i++) {
    if (buffer_[address + i] != kErasedValue) {
      PW_LOG_ERROR("Writing to previously written address: %zx",
                   size_t(address));
      return StatusWithSize::UNKNOWN;
    }
  }

  // Check for any injected write errors
  Status status = FlashError::Check(write_errors_, address, data.size());
  std::memcpy(&buffer_[address], data.data(), data.size());
  return StatusWithSize(status, data.size());
}

}  // namespace pw::kvs
