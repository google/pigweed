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

#include "pw_log/levels.h"

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

constexpr int convert_pigweed_to_android_log_level(int log_level) {
  switch (log_level) {
    case PW_LOG_LEVEL_DEBUG:
      return android_LogPriority::ANDROID_LOG_DEBUG;
    case PW_LOG_LEVEL_INFO:
      return android_LogPriority::ANDROID_LOG_INFO;
    case PW_LOG_LEVEL_WARN:
      return android_LogPriority::ANDROID_LOG_WARN;
    case PW_LOG_LEVEL_ERROR:
    case PW_LOG_LEVEL_CRITICAL:
      return android_LogPriority::ANDROID_LOG_ERROR;
    case PW_LOG_LEVEL_FATAL:
      return android_LogPriority::ANDROID_LOG_FATAL;
    default:
      return android_LogPriority::ANDROID_LOG_DEBUG;
  }
}

#define PW_HANDLE_LOG(level, module, flags, ...) \
  __android_log_print(                           \
      convert_pigweed_to_android_log_level(level), PW_LOG_TAG, __VA_ARGS__)
