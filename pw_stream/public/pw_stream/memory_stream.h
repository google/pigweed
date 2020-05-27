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

#include <array>
#include <cstddef>

#include "pw_span/span.h"
#include "pw_stream/stream.h"

namespace pw::stream {

class MemoryWriter : public Writer {
 public:
  MemoryWriter(span<std::byte> dest) : dest_(dest) {}

  size_t bytes_written() const { return bytes_written_; }

  span<const std::byte> WrittenData() const {
    return dest_.first(bytes_written_);
  }

  const std::byte* data() const { return dest_.data(); }

 private:
  // Implementation for writing data to this stream.
  //
  // If the in-memory buffer is exhausted in the middle of a write, this will
  // perform a partial write and Status::RESOURCE_EXHAUSTED will be returned.
  Status DoWrite(span<const std::byte> data) override;

  span<std::byte> dest_;
  size_t bytes_written_ = 0;
};

template <size_t size_bytes>
class MemoryWriterBuffer : public MemoryWriter {
 public:
  MemoryWriterBuffer() : MemoryWriter(buffer_) {}

 private:
  std::array<std::byte, size_bytes> buffer_;
};

}  // namespace pw::stream
