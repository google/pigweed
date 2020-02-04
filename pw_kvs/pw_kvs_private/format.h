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

#include "pw_kvs/checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_span/span.h"

namespace pw::kvs {

// EntryHeader represents a key-value entry as stored in flash.
class EntryHeader {
 public:
  EntryHeader() = default;

  EntryHeader(uint32_t magic,
              ChecksumAlgorithm* algorithm,
              std::string_view key,
              span<const std::byte> value,
              uint32_t key_version);

  Status VerifyChecksum(ChecksumAlgorithm* algorithm,
                        std::string_view key,
                        span<const std::byte> value) const;

  Status VerifyChecksumInFlash(FlashPartition* partition,
                               FlashPartition::Address header_address,
                               ChecksumAlgorithm* algorithm) const;

  size_t entry_size() const {
    return sizeof(*this) + key_length() + value_length();
  }

  uint32_t magic() const { return magic_; }

  uint32_t checksum() const { return checksum_; }

  constexpr size_t key_length() const {
    return key_value_length_ & kKeyLengthMask;
  }
  void set_key_length(uint32_t key_length) {
    key_value_length_ = key_length | (~kKeyLengthMask & key_value_length_);
  }

  constexpr size_t value_length() const {
    return key_value_length_ >> kValueLengthShift;
  }
  void set_value_length(uint32_t value_length) {
    key_value_length_ = (value_length << kValueLengthShift) |
                        (kKeyLengthMask & key_value_length_);
  }

  uint32_t key_version() const { return key_version_; }

 private:
  static constexpr uint32_t kNoChecksum = 0;
  static constexpr uint32_t kKeyLengthMask = 0b111111;
  static constexpr uint32_t kValueLengthShift = 8;

  static constexpr size_t checked_data_offset() {
    return offsetof(EntryHeader, key_value_length_);
  }

  span<const std::byte> checksum_bytes() const {
    return as_bytes(span(&checksum_, 1));
  }

  void CalculateChecksum(ChecksumAlgorithm* algorithm,
                         std::string_view key,
                         span<const std::byte> value) const;

  uint32_t magic_;
  uint32_t checksum_;
  //  6 bits, 0: 5 - key - maximum 64 characters
  //  2 bits, 6: 7 - reserved
  // 24 bits, 8:31 - value - maximum 16MB
  uint32_t key_value_length_;
  uint32_t key_version_;
};

static_assert(sizeof(EntryHeader) == 16, "EntryHeader should have no padding");

}  // namespace pw::kvs
