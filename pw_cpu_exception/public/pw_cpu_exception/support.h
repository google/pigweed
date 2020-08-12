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

// This facade provides an API for capturing the contents of a
// pw_CpuExceptionState struct in a platform-agnostic way. While this facade
// does not provide a means to directly access individual members of a
// pw_CpuExceptionState object, it does allow dumping CPU state without needing
// to know any specifics about the underlying architecture.
#pragma once

#include <cstdint>
#include <span>

// Forward declaration of pw_CpuExceptionState. Definition provided by backend.
struct pw_CpuExceptionState;

namespace pw::cpu_exception {

// Gets raw CPU state as a single contiguous block of data. The particular
// contents will depend on the specific backend and platform.
std::span<const uint8_t> RawFaultingCpuState(
    const pw_CpuExceptionState& cpu_state);

// Writes CPU state as a formatted string to a string builder.
// NEVER depend on the format of this output. This is exclusively FYI human
// readable output.
void ToString(const pw_CpuExceptionState& cpu_state,
              const std::span<char>& dest);

// Logs captured CPU state using pw_log at PW_LOG_LEVEL_INFO.
void LogCpuState(const pw_CpuExceptionState& cpu_state);

}  // namespace pw::cpu_exception
