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
#include <span>

#include "pw_stream/stream.h"
#include "pw_sys_io/sys_io.h"

namespace pw::stream {

class SerialWriter : public Writer {
 public:
  size_t bytes_written() const { return bytes_written_; }

 private:
  // Implementation for writing data to this stream.
  Status DoWrite(std::span<const std::byte> data) override {
    bytes_written_ += data.size_bytes();
    return pw::sys_io::WriteBytes(data).status();
  }

  size_t bytes_written_ = 0;
};

}  // namespace pw::stream
