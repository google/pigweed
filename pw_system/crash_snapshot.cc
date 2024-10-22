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

#include "pw_system/crash_snapshot.h"

#include <optional>

#include "pw_multisink/util.h"
#include "pw_snapshot/uuid.h"
#include "pw_status/status.h"
#include "pw_system/device_handler.h"
#include "pw_system/log.h"

namespace pw::system {

namespace {

CrashSnapshot crash_snapshot;

PW_PLACE_IN_SECTION(".noinit")
CrashSnapshotPersistentBuffer persistent_crash_snapshot;

std::byte submessage_scratch_buffer
    [snapshot::pwpb::Snapshot::kScratchBufferSizeBytes];

}  // namespace

CrashSnapshotPersistentBuffer& GetCrashSnapshotBuffer() {
  return persistent_crash_snapshot;
}

bool HasCrashSnapshot() { return persistent_crash_snapshot.has_value(); }

CrashSnapshot::CrashSnapshot()
    : writer_(persistent_crash_snapshot.GetWriter()) {}

void CrashSnapshot::Capture(const pw_cpu_exception_State& cpu_state,
                            const std::string_view reason) {
  // clear any old snapshot data prior to populating a new crash snapshot.
  persistent_crash_snapshot.clear();

  snapshot::pwpb::Snapshot::StreamEncoder snapshot_encoder(
      writer_, submessage_scratch_buffer);

  // TODO: b/354772694 - handle encoder problems.  Potentially write to memory
  // and log on boot that the snapshot couldn't be written.
  Status status = OkStatus();
  status.Update(CaptureMetadata(reason, snapshot_encoder));
  status.Update(device_handler::CaptureCpuState(cpu_state, snapshot_encoder));
  status.Update(CaptureThreads(cpu_state, snapshot_encoder));
  status.Update(CaptureLogs(snapshot_encoder));
  status.Update(snapshot_encoder.status());
}

Status CrashSnapshot::CaptureMetadata(
    const std::string_view reason,
    snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder) {
  snapshot::pwpb::Metadata::StreamEncoder metadata_encoder =
      snapshot_encoder.GetMetadataEncoder();

  // TODO: b/354770559 - generate a snapshot UUID.
  std::optional<snapshot::ConstUuidSpan> snapshot_uuid;
  if (snapshot_uuid.has_value()) {
    // TODO: https://pwbug.dev/357138320 - Review IgnoreError calls in this
    // file.
    metadata_encoder.WriteSnapshotUuid(snapshot_uuid.value()).IgnoreError();
  }

  if (!reason.empty()) {
    metadata_encoder.WriteReason(as_bytes(span(reason))).IgnoreError();
  }

  metadata_encoder.WriteFatal(true).IgnoreError();

  // TODO: b/354775975 - populate the metadata with version, build uuid
  // and project name.

  device_handler::CapturePlatformMetadata(metadata_encoder);

  return metadata_encoder.status();
}

Status CrashSnapshot::CaptureThreads(
    const pw_cpu_exception_State& cpu_state,
    snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder) {
  thread::proto::pwpb::SnapshotThreadInfo::StreamEncoder* thread_info_encoder =
      static_cast<thread::proto::pwpb::SnapshotThreadInfo::StreamEncoder*>(
          static_cast<protobuf::StreamEncoder*>(&snapshot_encoder));
  return device_handler::CaptureThreads(cpu_state.extended.psp,
                                        *thread_info_encoder);
}

Status CrashSnapshot::CaptureLogs(
    snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder) {
  log::pwpb::LogEntries::StreamEncoder encoder(writer_, ByteSpan());
  multisink::UnsafeDumpMultiSinkLogs(GetMultiSink(), encoder).IgnoreError();
  return snapshot_encoder.status();
}

}  // namespace pw::system
