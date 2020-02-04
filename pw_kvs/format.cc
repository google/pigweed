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

namespace pw::kvs {

using std::byte;
using std::string_view;

EntryHeader::EntryHeader(uint32_t magic,
                         ChecksumAlgorithm* algorithm,
                         string_view key,
                         span<const byte> value,
                         uint32_t key_version)
    : magic_(magic),
      checksum_(kNoChecksum),
      key_value_length_(value.size() << kValueLengthShift |
                        (key.size() & kKeyLengthMask)),
      key_version_(key_version) {
  if (algorithm != nullptr) {
    CalculateChecksum(algorithm, key, value);
    std::memcpy(&checksum_,
                algorithm->state().data(),
                std::min(algorithm->size_bytes(), sizeof(checksum_)));
  }
}

Status EntryHeader::VerifyChecksum(ChecksumAlgorithm* algorithm,
                                   string_view key,
                                   span<const byte> value) const {
  if (algorithm == nullptr) {
    return checksum() == kNoChecksum ? Status::OK : Status::DATA_LOSS;
  }
  CalculateChecksum(algorithm, key, value);
  return algorithm->Verify(checksum_bytes());
}

Status EntryHeader::VerifyChecksumInFlash(
    FlashPartition* partition,
    FlashPartition::Address header_address,
    ChecksumAlgorithm* algorithm,
    string_view key) const {
  if (algorithm == nullptr) {
    return checksum() == kNoChecksum ? Status::OK : Status::DATA_LOSS;
  }

  CalculateChecksum(algorithm, key);

  // Read the value piece-by-piece into a small buffer.
  // TODO: This read may be unaligned. The partition can handle this, but
  // consider creating a API that skips the intermediate buffering.
  byte buffer[32];

  size_t bytes_to_read = value_length();
  FlashPartition::Address address =
      header_address + sizeof(*this) + key_length();

  while (bytes_to_read > 0u) {
    const size_t read_size = std::min(sizeof(buffer), bytes_to_read);
    TRY(partition->Read(address, read_size, buffer));
    address += read_size;
    algorithm->Update(buffer, read_size);
  }

  return algorithm->Verify(checksum_bytes());
}

void EntryHeader::CalculateChecksum(ChecksumAlgorithm* algorithm,
                                    const string_view key,
                                    span<const byte> value) const {
  algorithm->Reset();
  algorithm->Update(reinterpret_cast<const byte*>(this) +
                        offsetof(EntryHeader, key_value_length_),
                    sizeof(*this) - offsetof(EntryHeader, key_value_length_));
  algorithm->Update(as_bytes(span(key)));
  algorithm->Update(value);
}

}  // namespace pw::kvs
