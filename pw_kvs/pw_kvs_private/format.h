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

// This file defines classes for managing the in-flash format for KVS entires.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "pw_kvs/alignment.h"
#include "pw_kvs/checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_span/span.h"

namespace pw::kvs {

// EntryHeader represents a key-value entry as stored in flash.
class EntryHeader {
 public:
  static constexpr size_t kMinAlignmentBytes = 16;

  EntryHeader() = default;

  // Creates a new EntryHeader for a valid (non-deleted) entry.
  static EntryHeader Valid(uint32_t magic,
                           ChecksumAlgorithm* algorithm,
                           std::string_view key,
                           span<const std::byte> value,
                           size_t alignment_bytes,
                           uint32_t key_version) {
    return EntryHeader(magic,
                       algorithm,
                       key,
                       value,
                       value.size(),
                       alignment_bytes,
                       key_version);
  }

  // Creates a new EntryHeader for a tombstone entry, which marks a deleted key.
  static EntryHeader Tombstone(uint32_t magic,
                               ChecksumAlgorithm* algorithm,
                               std::string_view key,
                               size_t alignment_bytes,
                               uint32_t key_version) {
    return EntryHeader(magic,
                       algorithm,
                       key,
                       {},
                       kDeletedValueLength,
                       alignment_bytes,
                       key_version);
  }

  Status VerifyChecksum(ChecksumAlgorithm* algorithm,
                        std::string_view key,
                        span<const std::byte> value) const;

  Status VerifyChecksumInFlash(FlashPartition* partition,
                               FlashPartition::Address header_address,
                               ChecksumAlgorithm* algorithm) const;

  // Calculates the total size of an entry, including padding.
  static constexpr size_t size(size_t alignment_bytes,
                               std::string_view key,
                               span<const std::byte> value) {
    return AlignUp(sizeof(EntryHeader) + key.size() + value.size(),
                   alignment_bytes);
  }

  // Total size of this entry, including padding.
  size_t size() const { return AlignUp(content_size(), alignment_bytes()); }

  uint32_t magic() const { return magic_; }

  uint32_t checksum() const { return checksum_; }

  // The length of the key in bytes. Keys are not null terminated.
  size_t key_length() const { return key_length_bytes_; }

  static constexpr size_t max_key_length() { return kKeyLengthMask; }

  void set_key_length(uint32_t key_length) { key_length_bytes_ = key_length; }

  // The length of the value, which is 0 if this is a tombstone entry.
  size_t value_length() const { return deleted() ? 0u : value_length_bytes_; }

  static constexpr size_t max_value_length() { return 0xFFFE; }

  void set_value_length(uint16_t value_length) {
    value_length_bytes_ = value_length;
  }

  size_t alignment_bytes() const { return (alignment_units_ + 1) * 16; }

  uint32_t key_version() const { return key_version_; }

  // True if this is a tombstone entry.
  bool deleted() const { return value_length_bytes_ == kDeletedValueLength; }

 private:
  // The total size of the entry, excluding padding.
  size_t content_size() const {
    return sizeof(EntryHeader) + key_length() + value_length();
  }

  static constexpr uint32_t kNoChecksum = 0;
  static constexpr uint32_t kKeyLengthMask = 0b111111;
  static constexpr uint16_t kDeletedValueLength = 0xFFFF;

  EntryHeader(uint32_t magic,
              ChecksumAlgorithm* algorithm,
              std::string_view key,
              span<const std::byte> value,
              uint16_t value_length_bytes,
              size_t alignment_bytes,
              uint32_t key_version);

  static constexpr size_t checked_data_offset() {
    return offsetof(EntryHeader, alignment_units_);
  }

  span<const std::byte> checksum_bytes() const {
    return as_bytes(span(&checksum_, 1));
  }

  span<const std::byte> CalculateChecksum(ChecksumAlgorithm* algorithm,
                                          std::string_view key,
                                          span<const std::byte> value) const;

  static constexpr uint8_t alignment_bytes_to_units(size_t alignment_bytes) {
    return (alignment_bytes + 15) / 16 - 1;  // An alignment of 0 is invalid.
  }

  uint32_t magic_;
  uint32_t checksum_;

  // Stores the alignment in 16-byte units, starting from 16. To calculate the
  // number of bytes, add one to this number and multiply by 16.
  uint8_t alignment_units_;

  // The length of the key in bytes.
  //  6 bits, 0:5 - key - maximum 64 characters
  //  2 bits, 6:7 - reserved
  uint8_t key_length_bytes_;

  // Byte length of the value; maximum of 65534. The max uint16_t value (65535
  // or 0xFFFF) is reserved to indicate this is a tombstone (deleted) entry.
  uint16_t value_length_bytes_;

  // The version of the key. Monotonically increasing.
  uint32_t key_version_;
};

static_assert(sizeof(EntryHeader) == 16, "EntryHeader should have no padding");
static_assert(sizeof(EntryHeader) == EntryHeader::kMinAlignmentBytes);

}  // namespace pw::kvs
