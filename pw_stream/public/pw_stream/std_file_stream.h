// Copyright 2021 The Pigweed Authors
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

#include <fstream>

#include "pw_stream/stream.h"

namespace pw::stream {

// Wraps an std::ifstream with the Reader interface.
class StdFileReader : public stream::SeekableReader {
 public:
  StdFileReader(const char* path) : stream_(path, std::ios::binary) {}

  void Close() { stream_.close(); }

 private:
  StatusWithSize DoRead(ByteSpan dest) final;
  Status DoSeek(ssize_t offset, Whence origin) final;

  std::ifstream stream_;
};

// Wraps an std::ofstream with the Writer interface.
class StdFileWriter : public stream::SeekableWriter {
 public:
  StdFileWriter(const char* path)
      : stream_(path, std::ios::binary | std::ios::trunc) {}

  void Close() { stream_.close(); }

 private:
  Status DoWrite(ConstByteSpan data) final;
  Status DoSeek(ssize_t offset, Whence origin) final;

  std::ofstream stream_;
};

}  // namespace pw::stream
