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

//! `pw_log` backend that calls [`std::println!`] to emit log messages.  This
//! module is useful when you have an application or test running on the
//! development host and you want messages to be emitted to `stdout`.
//!
//! *Note*: This modules requires `std`.
//!
//! TODO: https://issues.pigweed.dev/issues/311232605 - Document
//! how to configure facade backends.
use pw_log_backend_api::LogLevel;
pub use pw_log_backend_println_macro::pw_logf_backend;

#[doc(hidden)]
pub const fn log_level_tag(level: LogLevel) -> &'static str {
    match level {
        LogLevel::Debug => "DBG",
        LogLevel::Info => "INF",
        LogLevel::Warn => "WRN",
        LogLevel::Error => "ERR",
        LogLevel::Critical => "CRT",
        LogLevel::Fatal => "FTL",
    }
}
