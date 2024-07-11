// Copyright 2024 The Pigweed Authors
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

#include "pw_system/config.h"
#include "pw_thread/thread.h"

// For now, pw_system:async only supports FreeRTOS or standard library threads.
//
// This file will be rewritten once the SEED-0128 generic thread creation APIs
// are available. Details of the threads owned by pw_system should be an
// internal implementation detail. If configuration is necessary, it can be
// exposed through regular config options, rather than requiring users to
// implement functions.

#if __has_include("FreeRTOS.h")

#include "FreeRTOS.h"
#include "pw_thread_freertos/options.h"
#include "task.h"

namespace pw::system {
namespace {

constexpr size_t ToWords(size_t bytes) {
  return (bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
}

}  // namespace

[[noreturn]] void StartScheduler() {
  vTaskStartScheduler();
  PW_UNREACHABLE;
}

// Low to high priorities.
enum class ThreadPriority : UBaseType_t {
  kDispatcher = tskIDLE_PRIORITY + 1,
  // TODO(amontanez): These should ideally be at different priority levels, but
  // there's synchronization issues when they are.
  kLog = kDispatcher,
  kRpc = kDispatcher,
  kTransfer = kDispatcher,
  kNumPriorities,
};

static_assert(static_cast<UBaseType_t>(ThreadPriority::kNumPriorities) <=
              configMAX_PRIORITIES);

const thread::Options& LogThreadOptions() {
  static thread::freertos::StaticContextWithStack<ToWords(
      kLogThreadStackSizeBytes)>
      context;
  static constexpr auto options =
      pw::thread::freertos::Options()
          .set_name("LogThread")
          .set_static_context(context)
          .set_priority(static_cast<UBaseType_t>(ThreadPriority::kLog));
  return options;
}

const thread::Options& RpcThreadOptions() {
  static thread::freertos::StaticContextWithStack<ToWords(
      kRpcThreadStackSizeBytes)>
      context;
  static constexpr auto options =
      pw::thread::freertos::Options()
          .set_name("RpcThread")
          .set_static_context(context)
          .set_priority(static_cast<UBaseType_t>(ThreadPriority::kRpc));
  return options;
}

const thread::Options& TransferThreadOptions() {
  static thread::freertos::StaticContextWithStack<ToWords(
      kTransferThreadStackSizeBytes)>
      context;
  static constexpr auto options =
      pw::thread::freertos::Options()
          .set_name("TransferThread")
          .set_static_context(context)
          .set_priority(static_cast<UBaseType_t>(ThreadPriority::kTransfer));
  return options;
}

const thread::Options& DispatcherThreadOptions() {
  static thread::freertos::StaticContextWithStack<ToWords(
      kDispatcherThreadStackSizeBytes)>
      context;
  static constexpr auto options =
      pw::thread::freertos::Options()
          .set_name("DispatcherThread")
          .set_static_context(context)
          .set_priority(static_cast<UBaseType_t>(ThreadPriority::kDispatcher));
  return options;
}

}  // namespace pw::system

#else  // STL

#include <chrono>
#include <thread>

#include "pw_thread_stl/options.h"

namespace pw::system {
namespace {

thread::stl::Options stl_thread_options;

}  // namespace

[[noreturn]] void StartScheduler() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::system_clock::duration::max());
  }
}

const thread::Options& LogThreadOptions() { return stl_thread_options; }

const thread::Options& RpcThreadOptions() { return stl_thread_options; }

const thread::Options& TransferThreadOptions() { return stl_thread_options; }

const thread::Options& DispatcherThreadOptions() { return stl_thread_options; }

}  // namespace pw::system

#endif  // __has_include("FreeRTOS.h")
