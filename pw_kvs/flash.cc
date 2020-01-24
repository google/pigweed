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

#include "pw_kvs/util/flash.h"

#include "pw_kvs/config.h"

namespace pw {

Status PaddedWrite(FlashPartition* partition,
                   FlashPartition::Address address,
                   const uint8_t* buffer,
                   uint16_t size) {
  RETURN_STATUS_IF(
      address % partition->GetAlignmentBytes() ||
          partition->GetAlignmentBytes() > cfg::kFlashUtilMaxAlignmentBytes,
      Status::INVALID_ARGUMENT);
  uint8_t alignment_buffer[cfg::kFlashUtilMaxAlignmentBytes] = {0};
  uint16_t aligned_bytes = size - size % partition->GetAlignmentBytes();
  RETURN_IF_ERROR(partition->Write(address, buffer, aligned_bytes));
  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    memcpy(alignment_buffer, &buffer[aligned_bytes], remaining_bytes);
    RETURN_IF_ERROR(partition->Write(address + aligned_bytes,
                                     alignment_buffer,
                                     partition->GetAlignmentBytes()));
  }
  return Status::OK;
}

Status UnalignedRead(FlashPartition* partition,
                     uint8_t* buffer,
                     FlashPartition::Address address,
                     uint16_t size) {
  RETURN_STATUS_IF(
      address % partition->GetAlignmentBytes() ||
          partition->GetAlignmentBytes() > cfg::kFlashUtilMaxAlignmentBytes,
      Status::INVALID_ARGUMENT);
  uint16_t aligned_bytes = size - size % partition->GetAlignmentBytes();
  RETURN_IF_ERROR(partition->Read(buffer, address, aligned_bytes));
  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    uint8_t alignment_buffer[cfg::kFlashUtilMaxAlignmentBytes];
    RETURN_IF_ERROR(partition->Read(alignment_buffer,
                                    address + aligned_bytes,
                                    partition->GetAlignmentBytes()));
    memcpy(&buffer[aligned_bytes], alignment_buffer, remaining_bytes);
  }
  return Status::OK;
}

}  // namespace pw
