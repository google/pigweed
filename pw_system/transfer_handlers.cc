// Copyright 2023 The Pigweed Authors
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
#include "pw_system/transfer_handlers.h"

namespace pw::system {

CrashSnapshotBufferTransfer::CrashSnapshotBufferTransfer(
    uint32_t id, CrashSnapshotPersistentBuffer& buffer)
    : transfer::ReadOnlyHandler(id), buffer_(buffer), reader_({}) {
  set_reader(reader_);
}

Status CrashSnapshotBufferTransfer::PrepareRead() {
  if (!buffer_.has_value()) {
    return Status::Unavailable();
  }

  // As seeking is not yet supported, reinitialize the reader from the start
  // of the snapshot buffer.
  new (&reader_)
      stream::MemoryReader(pw::ConstByteSpan{buffer_.data(), buffer_.size()});

  return OkStatus();
}

TraceBufferTransfer::TraceBufferTransfer(uint32_t id,
                                         TracePersistentBuffer& buffer)
    : transfer::ReadOnlyHandler(id), buffer_(buffer), reader_({}) {
  set_reader(reader_);
}

Status TraceBufferTransfer::PrepareRead() {
  if (!buffer_.has_value()) {
    return Status::Unavailable();
  }

  // As seeking is not yet supported, reinitialize the reader from the start
  // of the snapshot buffer.
  new (&reader_)
      stream::MemoryReader(pw::ConstByteSpan{buffer_.data(), buffer_.size()});

  return OkStatus();
}

}  // namespace pw::system
