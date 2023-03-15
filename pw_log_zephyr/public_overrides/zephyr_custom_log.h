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

#pragma once

#include <zephyr/sys/__assert.h>

// If static_assert wasn't defined by zephyr/sys/__assert.h that means it's not
// supported, just ignore it.
#ifndef static_assert
#define static_assert(...)
#endif

#include <pw_log/log.h>

#undef LOG_DBG
#undef LOG_INF
#undef LOG_WRN
#undef LOG_ERR

#define LOG_DBG(format, ...) PW_LOG_DEBUG(format, ##__VA_ARGS__)
#define LOG_INF(format, ...) PW_LOG_INFO(format, ##__VA_ARGS__)
#define LOG_WRN(format, ...) PW_LOG_WARN(format, ##__VA_ARGS__)
#define LOG_ERR(format, ...) PW_LOG_ERROR(format, ##__VA_ARGS__)
