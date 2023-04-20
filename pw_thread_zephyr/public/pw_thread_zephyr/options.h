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

#include "pw_assert/assert.h"
#include "pw_thread/thread.h"
#include "pw_thread_zephyr/config.h"
#include "pw_thread_zephyr/context.h"

namespace pw::thread::zephyr {

// pw::thread::Options for Zephyr RTOS.
//
// Example usage:
//
//   pw::thread::Thread example_thread(
//     pw::thread::zephyr::Options(static_example_thread_context)
//         .set_priority(kFooPriority),
//     example_thread_function, example_arg);
//
// TODO(aeremin): Add support for time slice configuration
//     (k_thread_time_slice_set when CONFIG_TIMESLICE_PER_THREAD=y).
// TODO(aeremin): Add support for thread name
//     (k_thread_name_set when CONFIG_THREAD_MONITOR is enabled).
class Options : public thread::Options {
 public:
  constexpr Options(StaticContext& context) : context_(&context) {}
  constexpr Options(const Options&) = default;
  constexpr Options(Options&&) = default;

  // Sets the priority for the Zephyr RTOS thread.
  // Lower priority values have a higher scheduling priority.
  constexpr Options& set_priority(int priority) {
    PW_DASSERT(priority <= config::kLowestSchedulerPriority);
    PW_DASSERT(priority >= config::kHighestSchedulerPriority);
    priority_ = priority;
    return *this;
  }

  // Sets the Zephyr RTOS native options
  // (https://docs.zephyrproject.org/latest/kernel/services/threads/index.html#thread-options)
  constexpr Options& set_native_options(uint32_t native_options) {
    native_options_ = native_options;
    return *this;
  }

 private:
  friend thread::Thread;

  int priority() const { return priority_; }
  uint32_t native_options() const { return native_options_; }
  StaticContext* static_context() const { return context_; }

  int priority_ = config::kDefaultPriority;
  uint32_t native_options_ = 0;
  StaticContext* context_ = nullptr;
};

}  // namespace pw::thread::zephyr
