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
#pragma once

#include "pw_persistent_ram/persistent_buffer.h"
#include "pw_system/config.h"
#include "pw_trace_tokenized/config.h"
#include "pw_transfer/transfer.h"

namespace pw::system {

using CrashSnapshotPersistentBuffer = persistent_ram::PersistentBuffer<
    PW_SYSTEM_CRASH_SNAPSHOT_MEMORY_SIZE_BYTES>;

// CrashSnapshotBufferTransfer handler to connect a crash snapshot transfer
// resource ID to a data stream
class CrashSnapshotBufferTransfer : public transfer::ReadOnlyHandler {
 public:
  CrashSnapshotBufferTransfer(uint32_t id,
                              CrashSnapshotPersistentBuffer& buffer);

  Status PrepareRead() final;

 private:
  CrashSnapshotPersistentBuffer& buffer_;
  stream::MemoryReader reader_;
};

using TracePersistentBuffer =
    persistent_ram::PersistentBuffer<PW_TRACE_BUFFER_SIZE_BYTES>;

// TraceBufferTransfer handler to connect a trace transfer
// resource ID to a data stream
class TraceBufferTransfer : public transfer::ReadOnlyHandler {
 public:
  TraceBufferTransfer(uint32_t id, TracePersistentBuffer& buffer);

  Status PrepareRead() final;

 private:
  TracePersistentBuffer& buffer_;
  stream::MemoryReader reader_;
};

}  // namespace pw::system
