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
#![no_std]

/// Pigweed's standard log levels
///
/// Values are limited to 3 bits, to fit within the protobuf definition of
/// LogEntry's line_level in pw_log_rpc. These levels correspond with the log
/// levels from Python's logging library, but have different values.  The
/// values match the C/C++ implementation of the `pw_log` module.
///
/// TODO: https://issues.pigweed.dev/issues/314168783 - Add documentation on
/// the meaning of the log levels once it is written for Pigweed in general.
#[repr(u8)]
pub enum LogLevel {
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    // Level 6 is not defined in order to match the protobuf definition.
    Fatal = 7,
}
