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
// Configuration macros for the tokenizer module.
#pragma once

#include <zephyr/kernel.h>

namespace pw::thread::zephyr::config {

inline constexpr int kDefaultPriority = CONFIG_PIGWEED_THREAD_DEFAULT_PRIORITY;
inline constexpr int kHighestSchedulerPriority =
    -CONFIG_PIGWEED_THREAD_NUM_COOP_PRIORITIES;
inline constexpr int kLowestSchedulerPriority =
    CONFIG_PIGWEED_THREAD_NUM_PREEMPT_PRIORITIES - 1;

static_assert(kDefaultPriority <= kLowestSchedulerPriority);
static_assert(kDefaultPriority >= kHighestSchedulerPriority);

}  // namespace pw::thread::zephyr::config
