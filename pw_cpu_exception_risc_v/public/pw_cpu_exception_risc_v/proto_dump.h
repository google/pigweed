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

#include "pw_cpu_exception_risc_v/cpu_state.h"
#include "pw_protobuf/encoder.h"
#include "pw_status/status.h"

namespace pw::cpu_exception::risc_v {

// Dumps the cpu state struct as a proto (defined in
// pw_cpu_exception_risc_v_protos/cpu_state.proto).
//
// Returns:
//   OK - Entire proto was written to the encoder.
//   RESOURCE_EXHAUSTED - Insufficient space to encode proto.
//   UNKNOWN - Some other proto encoding error occurred.
Status DumpCpuStateProto(protobuf::StreamEncoder& dest,
                         const pw_cpu_exception_State& cpu_state);

}  // namespace pw::cpu_exception::risc_v
