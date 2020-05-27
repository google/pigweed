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

#include "pw_stream/memory_stream.h"

#include <cstddef>
#include <cstring>

#include "pw_status/status_with_size.h"

namespace pw::stream {

Status MemoryWriter::DoWrite(span<const std::byte> data) {
  size_t bytes_to_write =
      std::min(data.size_bytes(), dest_.size_bytes() - bytes_written_);
  std::memcpy(dest_.data() + bytes_written_, data.data(), bytes_to_write);
  bytes_written_ += bytes_to_write;

  if (bytes_to_write != data.size_bytes()) {
    return Status::RESOURCE_EXHAUSTED;
  }
  return Status::OK;
}

}  // namespace pw::stream