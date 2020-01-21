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

#include "pw_kvs/flash.h"

#include <cstring>

namespace pw::kvs {
namespace cfg {

// Configure the maximum supported alignment for the flash utility functions.
constexpr size_t kFlashUtilMaxAlignmentBytes = 16;

}  // namespace cfg

Status PaddedWrite(FlashPartition* partition,
                   FlashPartition::Address address,
                   const uint8_t* buffer,
                   uint16_t size) {
  if (address % partition->GetAlignmentBytes() ||
      partition->GetAlignmentBytes() > cfg::kFlashUtilMaxAlignmentBytes) {
    return Status::INVALID_ARGUMENT;
  }
  uint8_t alignment_buffer[cfg::kFlashUtilMaxAlignmentBytes] = {0};
  uint16_t aligned_bytes = size - size % partition->GetAlignmentBytes();
  if (Status status = partition->Write(address, buffer, aligned_bytes);
      !status.ok()) {
    return status;
  }
  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    std::memcpy(alignment_buffer, &buffer[aligned_bytes], remaining_bytes);
    if (Status status = partition->Write(address + aligned_bytes,
                                         alignment_buffer,
                                         partition->GetAlignmentBytes());
        !status.ok()) {
      return status;
    }
  }
  return Status::OK;
}

Status UnalignedRead(FlashPartition* partition,
                     uint8_t* buffer,
                     FlashPartition::Address address,
                     uint16_t size) {
  if (address % partition->GetAlignmentBytes() ||
      partition->GetAlignmentBytes() > cfg::kFlashUtilMaxAlignmentBytes) {
    return Status::INVALID_ARGUMENT;
  }
  uint16_t aligned_bytes = size - size % partition->GetAlignmentBytes();
  if (Status status = partition->Read(buffer, address, aligned_bytes);
      !status.ok()) {
    return status;
  }
  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    uint8_t alignment_buffer[cfg::kFlashUtilMaxAlignmentBytes];
    if (Status status = partition->Read(alignment_buffer,
                                        address + aligned_bytes,
                                        partition->GetAlignmentBytes());
        !status.ok()) {
      return status;
    }
    memcpy(&buffer[aligned_bytes], alignment_buffer, remaining_bytes);
  }
  return Status::OK;
}

}  // namespace pw::kvs
