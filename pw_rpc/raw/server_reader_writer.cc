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

#include "pw_rpc/raw/server_reader_writer.h"

#include <cstring>

namespace pw::rpc {

Status RawServerReaderWriter::Write(ConstByteSpan response) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  if (buffer().Contains(response)) {
    return ReleasePayloadBuffer(response);
  }

  std::span<std::byte> buffer = AcquirePayloadBuffer();

  if (response.size() > buffer.size()) {
    ReleasePayloadBuffer();
    return Status::OutOfRange();
  }

  std::memcpy(buffer.data(), response.data(), response.size());
  return ReleasePayloadBuffer(buffer.first(response.size()));
}

}  // namespace pw::rpc
