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

// This file describes Pigweed's public user-facing logging API.
//
// THIS PUBLIC API IS NOT STABLE OR COMPLETE!
//
// Key functionality is still missing:
//
// - API for controlling verbosity at run time
// - API for querying if logging is enabled for the given level or flags
//
#pragma once

#include "pw_log/levels.h"
#include "pw_log/options.h"

// log_backend.h must ultimately resolve to a header that implements the macros
// required by the logging facade, as described below.
//
// Inputs: Macros the downstream user provides to control the logging system:
//
//   PW_LOG_MODULE_NAME
//     - The module name the backend should use
//
//   PW_LOG_LEVEL
//     - General log level setting. By default, logs below this level are
//       excluded from the build.
//
// Outputs: Macros log_backend.h is expected to provide:
//
//   PW_LOG(level, flags, fmt, ...)
//     - Required.
//       Level - An integer level as defined by pw_log/levels.h
//       Flags - Arbitrary flags the backend can leverage; user-defined.
//               Example: HAS_PII - A log has personally-identifying data
//               Example: HAS_DII - A log has device-identifying data
//               Example: RELIABLE_DELIVERY - Ask backend to ensure the
//               log is delivered; this may entail blocking other logs.
//               Example: BEST_EFFORT - Don't deliver this log if it
//               would mean blocking or dropping important-flagged logs
//
//   PW_LOG_DEBUG(fmt, ...)
//   PW_LOG_INFO(fmt, ...)
//   PW_LOG_WARN(fmt, ...)
//   PW_LOG_ERROR(fmt, ...)
//   PW_LOG_CRITICAL(fmt, ...)
//     - Optional. If not defined by the backend, the facade's default
//       implementation defines these in terms of PW_LOG().
//
#include "pw_log_backend/log_backend.h"

#ifndef PW_LOG
#define PW_LOG(level, flags, message, ...)               \
  do {                                                   \
    if (PW_LOG_ENABLE_IF(level, flags)) {                \
      PW_HANDLE_LOG(level, flags, message, __VA_ARGS__); \
    }                                                    \
  } while (0)
#endif  // PW_LOG

// For backends that elect to only provide the general PW_LOG() macro and not
// specialized versions, define the standard PW_LOG_<level>() macros in terms
// of the general PW_LOG().
#ifndef PW_LOG_DEBUG
#define PW_LOG_DEBUG(message, ...) \
  PW_LOG(PW_LOG_LEVEL_DEBUG, PW_LOG_DEFAULT_FLAGS, message, __VA_ARGS__)
#endif  // PW_LOG_DEBUG

#ifndef PW_LOG_INFO
#define PW_LOG_INFO(message, ...) \
  PW_LOG(PW_LOG_LEVEL_INFO, PW_LOG_DEFAULT_FLAGS, message, __VA_ARGS__)
#endif  // PW_LOG_INFO

#ifndef PW_LOG_WARN
#define PW_LOG_WARN(message, ...) \
  PW_LOG(PW_LOG_LEVEL_WARN, PW_LOG_DEFAULT_FLAGS, message, __VA_ARGS__)
#endif  // PW_LOG_WARN

#ifndef PW_LOG_ERROR
#define PW_LOG_ERROR(message, ...) \
  PW_LOG(PW_LOG_LEVEL_ERROR, PW_LOG_DEFAULT_FLAGS, message, __VA_ARGS__)
#endif  // PW_LOG_ERROR

#ifndef PW_LOG_CRITICAL
#define PW_LOG_CRITICAL(message, ...) \
  PW_LOG(PW_LOG_LEVEL_CRITICAL, PW_LOG_DEFAULT_FLAGS, message, __VA_ARGS__)
#endif  // PW_LOG_CRITICAL

// Default: Number of bits available for the log level
//
// All log statements have a level, and this define is the number of bits
// available for the level. Some backends restrict this for better efficiency.
// By default, pick a restricted but large enough value to work for most cases.
#ifndef PW_LOG_LEVEL_BITS
#define PW_LOG_LEVEL_BITS 6
#endif  // PW_LOG_LEVEL_BITS

// Default: Number of bits available for the log flags
//
// All log statements have a flags field, and this define is the number of bits
// available for the flags. Some backends restrict this for better efficiency.
// By default, pick a restricted but large enough value to work for most cases.
#ifndef PW_LOG_FLAG_BITS
#define PW_LOG_FLAG_BITS 10
#endif  // PW_LOG_FLAG_BITS

// Define short, usable names if requested.
// TODO(pwbug/17): Convert this to the config system when available.
#ifndef PW_LOG_USE_SHORT_NAMES
#define PW_LOG_USE_SHORT_NAMES 0
#endif

// Define ultra short, usable names if requested.
#ifndef PW_LOG_USE_ULTRA_SHORT_NAMES
#define PW_LOG_USE_ULTRA_SHORT_NAMES 0
#endif  // PW_LOG_USE_SHORT_NAMES

// clang-format off
#if PW_LOG_USE_SHORT_NAMES
#define LOG          PW_LOG
#define LOG_DEBUG    PW_LOG_DEBUG
#define LOG_INFO     PW_LOG_INFO
#define LOG_WARN     PW_LOG_WARN
#define LOG_ERROR    PW_LOG_ERROR
#define LOG_CRITICAL PW_LOG_CRITICAL
#endif  // PW_LOG_USE_SHORT_NAMES
// clang-format on

// clang-format off
#if PW_LOG_USE_ULTRA_SHORT_NAMES
#if !PW_LOG_USE_SHORT_NAMES
#define LOG PW_LOG
#endif  // LOG
#define DBG PW_LOG_DEBUG
#define INF PW_LOG_INFO
#define WRN PW_LOG_WARN
#define ERR PW_LOG_ERROR
#define CRT PW_LOG_CRITICAL
#endif  // PW_LOG_USE_ULTRA_SHORT_NAMES
// clang-format on
