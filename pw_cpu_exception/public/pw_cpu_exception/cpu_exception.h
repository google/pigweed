// Copyright 2019 The Pigweed Authors
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

// Platform-independent mechanism to catch hardware CPU faults in user code.
// This module encapsulates low level CPU exception handling assembly for the
// platform. After early exception handling completes, this module invokes
// the following user-defined function:
//
//   pw::cpu_exception::HandleCpuException(CpuState* state)
//
// If platform-dependent access to the CPU registers is needed, then
// applications can include the respective backend module directly; for example
// cpu_exception_armv7m.
//
// IMPORTANT: To use this module, you MUST implement HandleCpuException() in
//            some part of your application.

#include <cstdint>
#include <span>

#include "pw_preprocessor/compiler.h"
#include "pw_string/string_builder.h"

namespace pw::cpu_exception {

// Forward declaration of CpuState. Definition provided by backend.
struct CpuState;

// Gets raw CPU state as a single contiguous block of data. The particular
// contents will depend on the specific backend and platform.
std::span<const uint8_t> RawFaultingCpuState(const CpuState& cpu_state);

// Writes CPU state as a formatted string to a string builder.
// NEVER depend on the format of this output. This is exclusively FYI human
// readable output.
void ToString(const CpuState& cpu_state, StringBuilder* builder);

// Application-defined recoverable CPU exception handler.
//
// Applications must define this function; it is not defined by the exception
// handler backend. After CPU state is captured by the cpu exception backend,
// this function is called. Applications can then choose to either gracefully
// handle the issue and return, or decide the exception cannot be handled and
// abort normal execution (e.g. reset).
//
// Examples of what applications could do in the handler: gracefully recover
// (e.g. enabling a floating point unit after triggering an exception executing
// a floating point instruction), reset the device, or wait for a debugger to
// attach.
//
// See the cpu_exception module documentation for more details.
PW_USED void HandleCpuException(CpuState* state);

// Low-level raw exception entry handler.
//
// Captures faulting CPU state into a platform-specific CpuState object, then
// calls the user-supplied HandleCpuException() fault handler.
//
// This function should be called immediately after a fault; typically by being
// in the interrupt vector table entries for the hard fault exceptions.
//
// Note: applications should almost never invoke this directly; if you do, make
// sure you know what you are doing.
extern "C" PW_NO_PROLOGUE void pw_CpuExceptionEntry(void);

}  // namespace pw::cpu_exception
