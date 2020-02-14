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

#include "pw_kvs/alignment.h"

namespace pw {

StatusWithSize AlignedWriter::Write(span<const std::byte> data) {
  while (!data.empty()) {
    size_t to_copy = std::min(write_size_ - bytes_in_buffer_, data.size());

    std::memcpy(&buffer_[bytes_in_buffer_], data.data(), to_copy);
    data = data.subspan(to_copy);
    bytes_in_buffer_ += to_copy;

    // If the buffer is full, write it out.
    if (bytes_in_buffer_ == write_size_) {
      StatusWithSize result = output_.Write(buffer_, write_size_);

      // Always use write_size_ for the bytes written. If there was an error
      // assume the space was written or at least disturbed.
      bytes_written_ += write_size_;
      if (!result.ok()) {
        return StatusWithSize(result.status(), bytes_written_);
      }

      bytes_in_buffer_ = 0;
    }
  }

  return StatusWithSize(bytes_written_);
}

StatusWithSize AlignedWriter::Flush() {
  static constexpr std::byte kPadByte = std::byte{0};

  // If data remains in the buffer, pad it to the alignment size and flush the
  // remaining data.
  if (bytes_in_buffer_ != 0u) {
    const size_t remaining_bytes = AlignUp(bytes_in_buffer_, alignment_bytes_);
    std::memset(&buffer_[bytes_in_buffer_],
                int(kPadByte),
                remaining_bytes - bytes_in_buffer_);

    if (auto result = output_.Write(buffer_, remaining_bytes); !result.ok()) {
      return StatusWithSize(result.status(), bytes_written_);
    }

    bytes_written_ += remaining_bytes;  // Include padding in the total.
  }

  const StatusWithSize result(bytes_written_);
  bytes_written_ = 0;
  bytes_in_buffer_ = 0;
  return result;
}

}  // namespace pw
