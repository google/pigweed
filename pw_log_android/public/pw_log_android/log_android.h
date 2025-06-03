// Copyright 2022 The Pigweed Authors
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

#include <android/log.h>

// This backend supports PW_LOG_MODULE_NAME as a fallback for Android logging's
// LOG_TAG if and only if LOG_TAG is not already set. We cannot directly set
// LOG_TAG here because it may be defined after this header is included. We
// use PW_LOG_TAG here instead.
#if defined(LOG_TAG)
#define PW_LOG_TAG LOG_TAG
#elif defined(PW_LOG_MODULE_NAME)
#define PW_LOG_TAG PW_LOG_MODULE_NAME
#else
#error \
    "Cannot set PW_LOG_TAG because LOG_TAG and PW_LOG_MODULE_NAME are not defined."
#endif  // defined(LOG_TAG)

// #define PW_LOG_LEVEL_DEBUG    1
#define _PW_LOG_ANDROID_LEVEL_1(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_DEBUG, PW_LOG_TAG, __VA_ARGS__)

// #define PW_LOG_LEVEL_INFO    2
#define _PW_LOG_ANDROID_LEVEL_2(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_INFO, PW_LOG_TAG, __VA_ARGS__)

// #define PW_LOG_LEVEL_WARN    3
#define _PW_LOG_ANDROID_LEVEL_3(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_WARN, PW_LOG_TAG, __VA_ARGS__)

// #define PW_LOG_LEVEL_ERROR    4
#define _PW_LOG_ANDROID_LEVEL_4(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_ERROR, PW_LOG_TAG, __VA_ARGS__)

// #define PW_LOG_LEVEL_CRITICAL    5
#define _PW_LOG_ANDROID_LEVEL_5(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_ERROR, PW_LOG_TAG, __VA_ARGS__)

// #define PW_LOG_LEVEL_FATAL    7
#define _PW_LOG_ANDROID_LEVEL_7(...) \
  __android_log_print(               \
      android_LogPriority::ANDROID_LOG_FATAL, PW_LOG_TAG, __VA_ARGS__)

#define _PW_HANDLE_LOG(level, module, flags, ...) \
  _PW_LOG_ANDROID_LEVEL_##level(__VA_ARGS__)

// The indirection through _PW_HANDLE_LOG ensures the `level` argument is
// expanded.
#define PW_HANDLE_LOG(...) _PW_HANDLE_LOG(__VA_ARGS__)
