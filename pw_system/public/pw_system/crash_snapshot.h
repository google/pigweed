// Copyright 2024 The Pigweed Authors
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

#include "pw_cpu_exception/state.h"
#include "pw_persistent_ram/persistent_buffer.h"
#include "pw_snapshot_protos/snapshot.pwpb.h"
#include "pw_system/transfer_handlers.h"

namespace pw::system {

CrashSnapshotPersistentBuffer& GetCrashSnapshotBuffer();
bool HasCrashSnapshot();

// CrashSnapshot is the main entry point for populating a crash snapshot.
// Information common to pw_system such as logs will be captured directly by
// this class, and any device specific information such as cpu_state and
// back traces will be delegated to the device backend handler.
class CrashSnapshot {
 public:
  CrashSnapshot();

  void Capture(const pw_cpu_exception_State& cpu_state,
               const std::string_view reason);

 private:
  Status CaptureMetadata(
      const std::string_view reason,
      snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder);

  Status CaptureMainStackThread(
      const pw_cpu_exception_State& cpu_state,
      snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder);

  Status CaptureThreads(
      const pw_cpu_exception_State& cpu_state,
      snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder);

  Status CaptureLogs(snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder);

 private:
  persistent_ram::PersistentBufferWriter writer_;
};

}  // namespace pw::system
