// Copyright 2022 The Pigweed Authors
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
#include "pw_system/device_handler.h"

#include "hardware/watchdog.h"
#include "pw_cpu_exception/state.h"
#include "pw_cpu_exception_cortex_m/snapshot.h"

namespace pw::system::device_handler {

void RebootSystem() { watchdog_reboot(0, 0, 0); }

void CapturePlatformMetadata(
    snapshot::pwpb::Metadata::StreamEncoder& metadata_encoder) {
// The device_handler is shared between rp2040 and 2350, so handle differences
// with the preprocessor.
#if _PW_ARCH_ARM_V6M
  // TODO: https://pwbug.dev/357132837 - Review if IgnoreError is correct here.
  metadata_encoder.WriteCpuArch(snapshot::pwpb::CpuArchitecture::Enum::ARMV6M)
      .IgnoreError();
#elif _PW_ARCH_ARM_V8M_MAINLINE || _PW_ARCH_ARM_V8_1M_MAINLINE
  // TODO: https://pwbug.dev/357132837 - Review if IgnoreError is correct here.
  metadata_encoder.WriteCpuArch(snapshot::pwpb::CpuArchitecture::Enum::ARMV8M)
      .IgnoreError();
#else
#error "Unknown CPU architecture."
#endif  // _PW_ARCH_ARM_V6M
}

Status CaptureCpuState(
    const pw_cpu_exception_State& cpu_state,
    snapshot::pwpb::Snapshot::StreamEncoder& snapshot_encoder) {
  return cpu_exception::cortex_m::SnapshotCpuState(
      cpu_state,
      *static_cast<cpu_exception::cortex_m::pwpb::SnapshotCpuStateOverlay::
                       StreamEncoder*>(
          static_cast<protobuf::StreamEncoder*>(&snapshot_encoder)));
}

}  // namespace pw::system::device_handler
