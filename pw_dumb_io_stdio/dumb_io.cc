// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pw_dumb_io/dumb_io.h"

#include <cstdio>

namespace pw::dumb_io {

Status GetByte(std::byte* dest) {
  if (dest == nullptr) {
    return Status::FAILED_PRECONDITION;
  }

  int value = std::getchar();
  if (value == EOF) {
    return Status::RESOURCE_EXHAUSTED;
  }
  *dest = static_cast<std::byte>(value);
  return Status::OK;
}

Status PutByte(std::byte b) {
  if (std::putchar(static_cast<char>(b)) == EOF) {
    return Status::INTERNAL;
  }
  return Status::OK;
}

}  // namespace pw::dumb_io
