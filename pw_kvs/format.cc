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

#include <cinttypes>

#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"

namespace pw::kvs {

using std::byte;
using std::string_view;

Entry::Entry(uint32_t magic,
             ChecksumAlgorithm* algorithm,
             string_view key,
             span<const byte> value,
             uint16_t value_length_bytes,
             size_t alignment_bytes,
             uint32_t key_version)
    : header_{.magic = magic,
              .checksum = 0,
              .alignment_units = alignment_bytes_to_units(alignment_bytes),
              .key_length_bytes = static_cast<uint8_t>(key.size()),
              .value_length_bytes = value_length_bytes,
              .key_version = key_version} {
  if (algorithm != nullptr) {
    span<const byte> checksum = CalculateChecksum(algorithm, key, value);
    std::memcpy(&header_.checksum,
                checksum.data(),
                std::min(checksum.size(), sizeof(header_.checksum)));
  }

  // TODO: 0 is an invalid alignment value. There should be an assert for this.
  // DCHECK_NE(alignment_bytes, 0);
}

Status Entry::VerifyChecksum(ChecksumAlgorithm* algorithm,
                             string_view key,
                             span<const byte> value) const {
  if (algorithm == nullptr) {
    return checksum() == 0 ? Status::OK : Status::DATA_LOSS;
  }
  CalculateChecksum(algorithm, key, value);
  return algorithm->Verify(checksum_bytes());
}

Status Entry::VerifyChecksumInFlash(FlashPartition* partition,
                                    FlashPartition::Address address,
                                    ChecksumAlgorithm* algorithm) const {
  // Read the entire entry piece-by-piece into a small buffer. If the entry is
  // 32 B or less, only one read is required.
  union {
    EntryHeader header_to_verify;
    byte buffer[sizeof(EntryHeader) * 2];
  };

  size_t bytes_to_read = size();
  size_t read_size = std::min(sizeof(buffer), bytes_to_read);

  // Read the first chunk, which includes the header, and compare the checksum.
  TRY(partition->Read(address, read_size, buffer));

  if (header_to_verify.checksum != checksum()) {
    PW_LOG_ERROR("Expected checksum %08" PRIx32 ", found %08" PRIx32,
                 checksum(),
                 header_to_verify.checksum);
    return Status::DATA_LOSS;
  }

  if (algorithm == nullptr) {
    return Status::OK;
  }

  // The checksum is calculated as if the header's checksum field were 0.
  header_to_verify.checksum = 0;

  algorithm->Reset();

  while (true) {
    // Add the chunk in the buffer to the checksum.
    algorithm->Update(buffer, read_size);

    bytes_to_read -= read_size;
    if (bytes_to_read == 0u) {
      break;
    }

    // Read the next chunk into the buffer.
    address += read_size;
    read_size = std::min(sizeof(buffer), bytes_to_read);
    TRY(partition->Read(address, read_size, buffer));
  }

  algorithm->Finish();
  return algorithm->Verify(checksum_bytes());
}

span<const byte> Entry::CalculateChecksum(ChecksumAlgorithm* algorithm,
                                          const string_view key,
                                          span<const byte> value) const {
  algorithm->Reset();

  {
    EntryHeader header_for_checksum = header_;
    header_for_checksum.checksum = 0;

    algorithm->Update(&header_for_checksum, sizeof(header_for_checksum));
    algorithm->Update(as_bytes(span(key)));
    algorithm->Update(value);
  }

  // Update the checksum with 0s to pad the entry to its alignment boundary.
  constexpr byte padding[kMinAlignmentBytes - 1] = {};
  size_t padding_to_add = Padding(content_size(), alignment_bytes());

  while (padding_to_add > 0u) {
    const size_t chunk_size = std::min(padding_to_add, sizeof(padding));
    algorithm->Update(padding, chunk_size);
    padding_to_add -= chunk_size;
  }

  return algorithm->Finish();
}

}  // namespace pw::kvs
