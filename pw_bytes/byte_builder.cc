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

#include "pw_bytes/byte_builder.h"

namespace pw {

void ByteBuilder::clear() {
  size_ = 0;
  status_ = Status::OK;
  last_status_ = Status::OK;
}

ByteBuilder& ByteBuilder::append(size_t count, std::byte b) {
  std::byte* const append_destination = &buffer_[size_];

  std::memset(append_destination, static_cast<int>(b), ResizeForAppend(count));
  return *this;
}

ByteBuilder& ByteBuilder::append(const void* bytes, size_t count) {
  std::byte* const append_destination = &buffer_[size_];
  std::memcpy(append_destination, bytes, ResizeForAppend(count));
  return *this;
}

size_t ByteBuilder::ResizeForAppend(size_t bytes_to_append) {
  const size_t copied = std::min(bytes_to_append, max_size() - size());
  size_ += copied;

  if (buffer_.empty() || bytes_to_append != copied) {
    SetErrorStatus(Status::RESOURCE_EXHAUSTED);
  } else {
    last_status_ = Status::OK;
  }

  return copied;
}

void ByteBuilder::resize(size_t new_size) {
  if (new_size <= size_) {
    size_ = new_size;
    last_status_ = Status::OK;
  } else {
    SetErrorStatus(Status::OUT_OF_RANGE);
  }
}

void ByteBuilder::CopySizeAndStatus(const ByteBuilder& other) {
  size_ = other.size_;
  status_ = other.status_;
  last_status_ = other.last_status_;
}

void ByteBuilder::SetErrorStatus(Status status) {
  last_status_ = status;
  status_ = status;
}

}  // namespace pw
