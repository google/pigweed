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
#pragma once

#include <cstddef>
#include <cstdint>

namespace pw::kvs::internal {

// Tracks the available and used space in each sector used by the KVS.
class SectorDescriptor {
 public:
  explicit constexpr SectorDescriptor(uint16_t sector_size_bytes)
      : tail_free_bytes_(sector_size_bytes), valid_bytes_(0) {}

  SectorDescriptor(const SectorDescriptor&) = default;
  SectorDescriptor& operator=(const SectorDescriptor&) = default;

  // The number of bytes available to be written in this sector.
  size_t writable_bytes() const { return tail_free_bytes_; }

  void set_writable_bytes(uint16_t writable_bytes) {
    tail_free_bytes_ = writable_bytes;
  }

  // The number of bytes of valid data in this sector.
  size_t valid_bytes() const { return valid_bytes_; }

  // Adds valid bytes without updating the writable bytes.
  void AddValidBytes(uint16_t bytes) { valid_bytes_ += bytes; }

  // Removes valid bytes without updating the writable bytes.
  void RemoveValidBytes(uint16_t bytes) {
    if (bytes > valid_bytes()) {
      // TODO: use a DCHECK instead -- this is a programming error
      valid_bytes_ = 0;
    } else {
      valid_bytes_ -= bytes;
    }
  }

  // Removes writable bytes without updating the valid bytes.
  void RemoveWritableBytes(uint16_t bytes) {
    if (bytes > writable_bytes()) {
      // TODO: use a DCHECK instead -- this is a programming error
      tail_free_bytes_ = 0;
    } else {
      tail_free_bytes_ -= bytes;
    }
  }

  bool HasSpace(size_t required_space) const {
    return tail_free_bytes_ >= required_space;
  }

  bool Empty(size_t sector_size_bytes) const {
    return tail_free_bytes_ == sector_size_bytes;
  }

  // Returns the number of bytes that would be recovered if this sector is
  // garbage collected.
  size_t RecoverableBytes(size_t sector_size_bytes) const {
    return sector_size_bytes - valid_bytes_ - tail_free_bytes_;
  }

 private:
  uint16_t tail_free_bytes_;  // writable bytes at the end of the sector
  uint16_t valid_bytes_;      // sum of sizes of valid entries
};

}  // namespace pw::kvs::internal
