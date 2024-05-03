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

//! `pw_log` is an extensible logging system that can delegate to
//! pre-existing logging APIs without upstream changes.
//!
//! Clients of `pw_log` simply import and use the logging API, and
//! log invocations will be handled by the provided logging backend.
//!
//! This flexibility is accomplished using Pigweed's
//! [facade pattern](https://pigweed.dev/docs/facades.html),
//! which uses build-system redirection to forward log invocations to the
//! configured backend implementaiton.
//!
//! ```
//! use pw_log::{logf, LogLevel};
//!
//! logf!(LogLevel::Info, "Thank you for signing up for Log Facts!");
//! logf!(LogLevel::Info, "Log Fact: Logs can be either %s, %s, or %s sawn.",
//!   "flat", "quarter", "rift");
//! ```
//!
//! Today `printf` style format strings are supported with Rust
//! [`core::fmt`]/`println!()` style strings planned
//! ([b/311232607](https://issues.pigweed.dev/issues/311232607)).
//!
//! TODO: <pwbug.dev/311266298> - Document `pw_log`'s backend API.
//!
//! TODO: <pwbug.dev/311232605> - Document how to configure facade backends.
#![cfg_attr(not(feature = "std"), no_std)]
#![deny(missing_docs)]

pub use pw_log_backend_api::LogLevel;

// Re-export dependences of `pw_log` macros to be accessed via `$crate::__private`.
#[doc(hidden)]
pub mod __private {
    pub use crate::*;
    pub use pw_log_backend;
    pub use pw_log_backend::pw_logf_backend;
}

/// Emit a log message using `printf` format string semantics.
///
/// `logf` takes a [`LogLevel`], a `printf style format string, and necessary
/// arguments to that string and emits a log message to the logging backend.
///
/// ```
/// use pw_log::{logf, LogLevel};
///
/// logf!(LogLevel::Info, "Log fact: A %s log has a Janka hardness of %d lbf.",
///     "Spruce Pine", 700);
/// ```
#[macro_export]
macro_rules! logf {
  ($log_level:expr, $format_string:literal) => {{
    use $crate::__private as __pw_log_crate;
    $crate::__private::pw_logf_backend!($log_level, $format_string)
  }};

  ($log_level:expr, $format_string:literal, $($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    $crate::__private::pw_logf_backend!($log_level, $format_string, $($args),*)
  }};
}

/// Deprecated alias for [`logf!`].
#[macro_export]
macro_rules! pw_logf {
  ($($args:expr),*) => {{
    logf!($($args),*)
  }}
}

/// Emit a debug level log message using `printf` format string semantics.
///
/// ```
/// use pw_log::{debugf, LogLevel};
///
/// debugf!("Log Fact: The American toy Lincoln Logs were inspired by the %s in %s.",
///     "Imperial Hotel", "Tokyo");
/// ```
#[macro_export]
macro_rules! debugf {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Debug, $($args),*)
  }};
}

/// Deprecated alias for [`debugf!`].
#[macro_export]
macro_rules! pw_log_debugf {
  ($($args:expr),*) => {{
    debugf!($($args),*)
  }}
}

/// Emit an info level log message using `printf` format string semantics.
///
/// ```
/// use pw_log::{infof, LogLevel};
///
/// infof!(
///     "Log Fact: The American president Abraham Lincoln (born %x) once lived in a log cabin.",
/// 0x1809);
/// ```
#[macro_export]
macro_rules! infof {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Info, $($args),*)
  }};
}

/// Deprecated alias for [`infof!`].
#[macro_export]
macro_rules! pw_log_infof {
  ($($args:expr),*) => {{
    infof!($($args),*)
  }}
}

/// Emit a warn level log message using `printf` format string semantics.
///
/// ```
/// use pw_log::{warnf, LogLevel};
///
/// warnf!(
///     "Log Fact: Made from a log, an %d year old dugout canoe is the oldest discovered boat in %s.",
///     8000, "Africa");
/// ```
#[macro_export]
macro_rules! warnf {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Warn, $($args),*)
  }};
}

/// Deprecated alias for [`warnf!`].
#[macro_export]
macro_rules! pw_log_warnf {
  ($($args:expr),*) => {{
    warnf!($($args),*)
  }}
}

/// Emit an error level log message using `printf` format string semantics.
///
/// ```
/// use pw_log::{errorf, LogLevel};
///
/// errorf!(
///     "Log Fact: Before saws were invented, the %s was used prepare logs for use.",
///     "adze");
/// ```
#[macro_export]
macro_rules! errorf {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Error, $($args),*)
  }};
}

/// Deprecated alias for [`errorf!`].
#[macro_export]
macro_rules! pw_log_errorf {
  ($($args:expr),*) => {{
    errorf!($($args),*)
  }}
}

/// Emit a critical level log message using `printf` format string semantics.
///
/// ```
/// use pw_log::{criticalf, LogLevel};
///
/// criticalf!(
///     "Log Fact: Until the %dth century, all ships' masts were made from a single log.",
///     19);
/// ```
#[macro_export]
macro_rules! criticalf {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Critical, $($args),*)
  }};
}

/// Deprecated alias for [`criticalf!`].
#[macro_export]
macro_rules! pw_log_criticalf {
  ($($args:expr),*) => {{
    criticalf!($($args),*)
  }}
}

/// Emit a fatal level log message using `printf` format string semantics.
///
/// *Note*: `fatalf` only emits a log message and does not cause a `panic!()`
///
/// ```
/// use pw_log::{fatalf, LogLevel};
///
/// fatalf!("Log Fact: All out of log facts! Timber!");
/// ```
#[macro_export]
macro_rules! fatalf {
  ($($args:expr),*) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf!(__pw_log_crate::LogLevel::Fatal, $($args),*)
  }};
}

/// Deprecated alias for [`fatalf!`].
#[macro_export]
macro_rules! pw_log_fatalf {
  ($($args:expr),*) => {{
    fatalf!($($args),*)
  }}
}

#[cfg(test)]
mod tests {
    // TODO(b/311262163): Add infrastructure for testing behavior of `pw_log` API.
    // The syntax of that API is verified through doctests.
}
