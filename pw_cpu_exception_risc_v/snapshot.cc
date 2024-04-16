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

#define PW_LOG_LEVEL PW_CPU_EXCEPTION_RISC_V_LOG_LEVEL

#include "pw_cpu_exception_risc_v/snapshot.h"

#include "pw_cpu_exception_risc_v/proto_dump.h"
#include "pw_cpu_exception_risc_v_protos/cpu_state.pwpb.h"
#include "pw_protobuf/encoder.h"
#include "pw_status/status.h"

namespace pw::cpu_exception::risc_v {

Status SnapshotCpuState(
    const pw_cpu_exception_State& cpu_state,
    SnapshotCpuStateOverlay::StreamEncoder& snapshot_encoder) {
  {
    RiscvCpuState::StreamEncoder cpu_state_encoder =
        snapshot_encoder.GetRiscvCpuStateEncoder();
    DumpCpuStateProto(cpu_state_encoder, cpu_state);
  }
  return snapshot_encoder.status();
}

}  // namespace pw::cpu_exception::risc_v
