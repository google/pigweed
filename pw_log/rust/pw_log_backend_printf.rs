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

//! `pw_log` backend that calls `libc`'s `printf` to emit log messages.  This
//! module is useful when you have a mixed C/C++ and Rust code base and want a
//! simple logging system that leverages an existing `printf` implementation.
//!
//! *Note*: This uses FFI to call `printf`.  This has two implications:
//! 1. C/C++ macro processing is not done.  If a system's `printf` relies on
//!    macros, this backend will likely need to be forked to make work.
//! 2. FFI calls use `unsafe`.  Attempts are made to use `printf` in sound ways
//!    such as bounding the length of strings explicitly but this is still an
//!    off-ramp from Rust's safety guarantees.
//!
//! TODO: <pwbug.dev/311232605> - Document how to configure facade backends.
pub use pw_log_backend_printf_macro::_pw_logf_backend;

// Re-export dependences of backend proc macro to be accessed via `$crate::__private`.
#[doc(hidden)]
pub mod __private {
    pub use pw_bytes::concat_static_strs;
    pub use pw_format_core::PrintfFormatter;
    use pw_log_backend_api::LogLevel;

    pub const fn log_level_tag(level: LogLevel) -> &'static str {
        match level {
            LogLevel::Debug => "DBG\0",
            LogLevel::Info => "INF\0",
            LogLevel::Warn => "WRN\0",
            LogLevel::Error => "ERR\0",
            LogLevel::Critical => "CRT\0",
            LogLevel::Fatal => "FTL\0",
        }
    }
}

#[macro_export]
macro_rules! pw_logf_backend {
    ($log_level:expr, $format_string:literal $(, $args:expr)* $(,)?) => {{
        use $crate::__private as __pw_log_backend_crate;
        $crate::_pw_logf_backend!($log_level, $format_string,  $($args),*)
    }};
}
