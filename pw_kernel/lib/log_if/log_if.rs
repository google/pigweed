// Copyright 2025 The Pigweed Authors
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

pub use pw_log_backend_api::LogLevel;

// Re-export dependencies of `pw_log` macros to be accessed via `$crate::__private`.
#[doc(hidden)]
pub mod __private {
    pub use pw_log_backend::{pw_log_backend, pw_logf_backend};

    pub use crate::*;
}

/// Emit a log message using `core::fmt` format string semantics if condition is true.
///
/// `log_if` takes an `expr` condition, a [`LogLevel`], a `core::fmt` style format string,
/// and necessary arguments to that string and emits a log message to the logging backend.
///
/// ```
/// use pw_log::LogLevel;
/// use log_if::log_if;
///
/// const LOG_FACTS: bool = true;
/// log_if!(LOG_FACTS, LogLevel::Info, "Log fact: A {} log has a Janka hardness of {} lbf.",
///     "Spruce Pine" as &str, 700 as i32);
/// ```
#[macro_export]
macro_rules! log_if {
  ($condition:expr, $log_level:expr, $format_string:literal $(,)?) => {{
    if $condition {
      use $crate::__private as __pw_log_crate;
      $crate::__private::pw_log_backend!($log_level, $format_string)
    }
  }};

  ($condition:expr, $log_level:expr, $format_string:literal, $($args:expr),* $(,)?) => {{
    if $condition {
      use $crate::__private as __pw_log_crate;
      $crate::__private::pw_log_backend!($log_level, $format_string, $($args),*)
    }
  }};
}

/// Emit a log message using `printf` format string semantics if condition is true.
///
/// `logf_if` takes an `expr` condition, a [`LogLevel`], a `printf` style format string,
/// and necessary arguments to that string and emits a log message to the logging backend.
///
/// ```
/// use pw_log::LogLevel;
/// use log_if::logf_if;
///
/// const LOG_FACTS: bool = true;
/// logf_if!(LOG_FACTS, LogLevel::Info, "Log fact: A %s log has a Janka hardness of %d lbf.",
///     "Spruce Pine", 700);
/// ```
#[macro_export]
macro_rules! logf_if {
  ($condition:expr, $log_level:expr, $format_string:literal $(,)?) => {{
    if $condition {
      use $crate::__private as __pw_log_crate;
      $crate::__private::pw_logf_backend!($log_level, $format_string)
    }
  }};

  ($condition:expr, $log_level:expr, $format_string:literal, $($args:expr),* $(,)?) => {{
    if $condition {
      use $crate::__private as __pw_log_crate;
      $crate::__private::pw_logf_backend!($log_level, $format_string, $($args),*)
    }
  }};
}

/// Emit a debug level log message using `core:fmt` format string semantics if condition
/// is true.
///
/// ```
/// use log_if::debug_if;
///
/// const LOG_FACTS: bool = true;
/// debug_if!(LOG_FACTS, "Log Fact: The American toy Lincoln Logs were inspired by the {} in {}.",
///     "Imperial Hotel" as &str, "Tokyo" as &str);
/// ```
#[macro_export]
macro_rules! debug_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Debug, $($args),*)
  }};
}

/// Emit a debug level log message using `printf` format string semantics if condition
/// is true.
///
/// ```
/// use log_if::debugf_if;
///
/// const LOG_FACTS: bool = true;
/// debugf_if!(LOG_FACTS, "Log Fact: The American toy Lincoln Logs were inspired by the %s in %s.",
///     "Imperial Hotel", "Tokyo");
/// ```
#[macro_export]
macro_rules! debugf_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
      use $crate::__private as __pw_log_crate;
      __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Debug, $($args),*)
  }};
}

/// Emit an info level log message using `core:fmt` format string semantics if condition is true.
///
/// ```
/// use log_if::info_if;
///
/// const LOG_FACTS: bool = true;
/// info_if!(
///     LOG_FACTS,
///     "Log Fact: The American president Abraham Lincoln (born {:x}) once lived in a log cabin.",
///     0x1809 as u32);
/// ```
#[macro_export]
macro_rules! info_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Info, $($args),*)
  }};
}

/// Emit an info level log message using `printf` format string semantics if condition is true.
///
/// ```
/// use log_if::infof_if;
///
/// const LOG_FACTS: bool = true;
/// infof_if!(
///     LOG_FACTS,
///     "Log Fact: The American president Abraham Lincoln (born %x) once lived in a log cabin.",
///     0x1809);
/// ```
#[macro_export]
macro_rules! infof_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Info, $($args),*)
  }};
}

/// Emit a warn level log message using `core::fmt` format string semantics if condition is true.
///
/// ```
/// use log_if::warn_if;
///
/// const LOG_FACTS: bool = true;
/// warn_if!(
///     LOG_FACTS,
///     "Log Fact: Made from a log, an {} year old dugout canoe is the oldest discovered boat in {}.",
///     8000 as i32, "Africa" as &str);
/// ```
#[macro_export]
macro_rules! warn_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Warn, $($args),*)
  }};
}

/// Emit a warn level log message using `printf` format string semantics if condition is true.
///
/// ```
/// use log_if::warnf_if;
///
/// const LOG_FACTS: bool = true;
/// warnf_if!(
///     LOG_FACTS,
///     "Log Fact: Made from a log, an %d year old dugout canoe is the oldest discovered boat in %s.",
///     8000, "Africa");
/// ```
#[macro_export]
macro_rules! warnf_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Warn, $($args),*)
  }};
}

/// Emit an error level log message using `core::fmt` format string semantics if
/// condition is true.
///
/// ```
/// use log_if::error_if;
///
/// const LOG_FACTS: bool = true;
/// error_if!(
///     LOG_FACTS,
///     "Log Fact: Before saws were invented, the {} was used prepare logs for use.",
///     "adze" as &str);
/// ```
#[macro_export]
macro_rules! error_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Error, $($args),*)
  }};
}

/// Emit an error level log message using `printf` format string semantics if condition
/// is true.
///
/// ```
/// use log_if::errorf_if;
///
/// const LOG_FACTS: bool = true;
/// errorf_if!(
///     LOG_FACTS,
///     "Log Fact: Before saws were invented, the %s was used prepare logs for use.",
///     "adze");
/// ```
#[macro_export]
macro_rules! errorf_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Error, $($args),*)
  }};
}

/// Emit a critical level log message using `core::fmt` format string semantics if condition
/// is true.
///
/// ```
/// use log_if::critical_if;
///
/// const LOG_FACTS: bool = true;
/// critical_if!(
///     LOG_FACTS,
///     "Log Fact: Until the {}th century, all ships' masts were made from a single log.",
///     19 as u32);
/// ```
#[macro_export]
macro_rules! critical_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Critical, $($args),*)
  }};
}

/// Emit a critical level log message using `printf` format string semantics if condition
/// is true.
///
/// ```
/// use log_if::criticalf_if;
///
/// const LOG_FACTS: bool = true;
/// criticalf_if!(
///     LOG_FACTS,
///     "Log Fact: Until the %dth century, all ships' masts were made from a single log.",
///     19);
/// ```
#[macro_export]
macro_rules! criticalf_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Critical, $($args),*)
  }};
}

/// Emit a fatal level log message using `core::fmt` format string semantics if
/// condition is true.
///
/// *Note*: `fatal` only emits a log message and does not cause a `panic!()`
///
/// ```
/// use log_if::fatal_if;
///
/// const LOG_FACTS: bool = true;
/// fatal_if!(LOG_FACTS, "Log Fact: All out of log facts! Timber!");
/// ```
#[macro_export]
macro_rules! fatal_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
    use $crate::__private as __pw_log_crate;
    __pw_log_crate::log_if!($condition, __pw_log_crate::LogLevel::Fatal, $($args),*)
  }};
}

/// Emit a fatal level log message using `printf` format string semantics if
/// condition is true.
///
/// *Note*: `fatalf` only emits a log message and does not cause a `panic!()`
///
/// ```
/// use log_if::fatalf_if;
///
/// const LOG_FACTS: bool = true;
/// fatalf_if!(LOG_FACTS, "Log Fact: All out of log facts! Timber!");
/// ```
#[macro_export]
macro_rules! fatalf_if {
  ($condition:expr, $($args:expr),* $(,)?) => {{
      use $crate::__private as __pw_log_crate;
      __pw_log_crate::logf_if!($condition, __pw_log_crate::LogLevel::Fatal, $($args),*)
  }};
}
