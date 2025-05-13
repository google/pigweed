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

class Context;

// pw::thread::Options for Zephyr RTOS.
//
// Example usage:
//
//   pw::Thread example_thread(
//     pw::thread::zephyr::Options(static_example_thread_context)
//         .set_priority(kFooPriority)
//         .set_name("example_thread"),
//     example_thread_function, example_arg);
//
// TODO(aeremin): Add support for time slice configuration
//     (k_thread_time_slice_set when CONFIG_TIMESLICE_PER_THREAD=y).
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

  // Sets the name for the ThreadX thread, note that this will be deep copied
  // into the context and may be truncated based on
  // PW_THREAD_ZEPHYR_CONFIG_MAX_THREAD_NAME_LEN. This may be set natively
  // in the zephyr thread if CONFIG_THREAD_NAME is set, where it may
  // again be truncated based on the value of CONFIG_THREAD_MAX_NAME_LEN.
  constexpr Options& set_name(const char* name) {
    name_ = name;
    return *this;
  }

  // Sets the Zephyr RTOS native options
  // (https://docs.zephyrproject.org/latest/kernel/services/threads/index.html#thread-options)
  constexpr Options& set_native_options(uint32_t native_options) {
    native_options_ = native_options;
    return *this;
  }

 private:
  friend Thread;
  friend Context;
  // Note that the default name may end up truncated due to
  // PW_THREAD_ZEPHYR_CONFIG_MAX_THREAD_NAME_LEN.
  static constexpr char kDefaultName[] = "pw::Thread";

  const char* name() const { return name_; }
  int priority() const { return priority_; }
  uint32_t native_options() const { return native_options_; }
  StaticContext* static_context() const { return context_; }

  int priority_ = config::kDefaultPriority;
  uint32_t native_options_ = 0;
  StaticContext* context_ = nullptr;
  const char* name_ = kDefaultName;
};

}  // namespace pw::thread::zephyr
