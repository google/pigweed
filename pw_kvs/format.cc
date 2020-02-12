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

#include "pw_kvs_private/format.h"

#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {

using std::byte;
using std::string_view;

EntryHeader::EntryHeader(uint32_t magic,
                         ChecksumAlgorithm* algorithm,
                         string_view key,
                         span<const byte> value,
                         uint16_t value_length_bytes,
                         size_t alignment_bytes,
                         uint32_t key_version)
    : magic_(magic),
      checksum_(kNoChecksum),
      alignment_units_(alignment_bytes_to_units(alignment_bytes)),
      key_length_bytes_(key.size()),
      value_length_bytes_(value_length_bytes),
      key_version_(key_version) {
  if (algorithm != nullptr) {
    CalculateChecksum(algorithm, key, value);
    std::memcpy(&checksum_,
                algorithm->Finish().data(),
                std::min(algorithm->size_bytes(), sizeof(checksum_)));
  }

  // TODO: 0 is an invalid alignment value. There should be an assert for this.
  // DCHECK_NE(alignment_bytes, 0);
}

Status EntryHeader::VerifyChecksum(ChecksumAlgorithm* algorithm,
                                   string_view key,
                                   span<const byte> value) const {
  if (algorithm == nullptr) {
    return checksum() == kNoChecksum ? Status::OK : Status::DATA_LOSS;
  }
  CalculateChecksum(algorithm, key, value);
  algorithm->Finish();
  return algorithm->Verify(checksum_bytes());
}

Status EntryHeader::VerifyChecksumInFlash(FlashPartition* partition,
                                          FlashPartition::Address address,
                                          ChecksumAlgorithm* algorithm) const {
  // Read the entire entry piece-by-piece into a small buffer.
  // TODO: This read may be unaligned. The partition can handle this, but
  // consider creating a API that skips the intermediate buffering.
  byte buffer[32];

  // Read and compare the magic and checksum.
  TRY(partition->Read(address, checked_data_offset(), buffer));
  if (std::memcmp(this, buffer, checked_data_offset()) != 0) {
    static_assert(sizeof(unsigned) >= sizeof(uint32_t));
    unsigned actual_magic;
    std::memcpy(&actual_magic, &buffer[0], sizeof(uint32_t));
    unsigned actual_checksum;
    std::memcpy(&actual_checksum, &buffer[4], sizeof(uint32_t));

    PW_LOG_ERROR(
        "Expected: magic=%08x, checksum=%08x; "
        "Actual: magic=%08x, checksum=%08x",
        unsigned(magic()),
        unsigned(checksum()),
        actual_magic,
        actual_checksum);

    return Status::DATA_LOSS;
  }

  if (algorithm == nullptr) {
    return Status::OK;
  }

  algorithm->Reset();

  // Read and calculate the checksum of the remaining header, key, and value.
  address += checked_data_offset();
  size_t bytes_to_read = content_size() - checked_data_offset();

  while (bytes_to_read > 0u) {
    const size_t read_size = std::min(sizeof(buffer), bytes_to_read);

    TRY(partition->Read(address, read_size, buffer));
    algorithm->Update(buffer, read_size);

    address += read_size;
    bytes_to_read -= read_size;
  }
  algorithm->Finish();

  return algorithm->Verify(checksum_bytes());
}

void EntryHeader::CalculateChecksum(ChecksumAlgorithm* algorithm,
                                    const string_view key,
                                    span<const byte> value) const {
  algorithm->Reset();
  algorithm->Update(reinterpret_cast<const byte*>(this) + checked_data_offset(),
                    sizeof(*this) - checked_data_offset());
  algorithm->Update(as_bytes(span(key)));
  algorithm->Update(value);
}

}  // namespace pw::kvs
