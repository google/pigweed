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

#include "pw_thread/thread.h"
#include "pw_thread_zephyr/config.h"
#include "pw_thread_zephyr/options.h"

namespace pw::system {

using namespace pw::thread::zephyr::config;

// Low to high priorities.
enum class ThreadPriority : int {
  kWorkQueue = kDefaultPriority,
  // TODO(amontanez): These should ideally be at different priority levels, but
  // there's synchronization issues when they are.
  kLog = kWorkQueue,
  kRpc = kWorkQueue,
  kNumPriorities,
};

static constexpr size_t kLogThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_LOG_STACK_SIZE;
static thread::zephyr::StaticContextWithStack<kLogThreadStackWords>
    log_thread_context;
const thread::Options& LogThreadOptions() {
  static constexpr auto options =
      pw::thread::zephyr::Options(log_thread_context)
          .set_priority(static_cast<int>(ThreadPriority::kLog));
  return options;
}

static constexpr size_t kRpcThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_RPC_STACK_SIZE;
static thread::zephyr::StaticContextWithStack<kRpcThreadStackWords>
    rpc_thread_context;
const thread::Options& RpcThreadOptions() {
  static constexpr auto options =
      pw::thread::zephyr::Options(rpc_thread_context)
          .set_priority(static_cast<int>(ThreadPriority::kRpc));
  return options;
}

static constexpr size_t kWorkQueueThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_WORK_QUEUE_STACK_SIZE;
static thread::zephyr::StaticContextWithStack<kWorkQueueThreadStackWords>
    work_queue_thread_context;

const thread::Options& WorkQueueThreadOptions() {
  static constexpr auto options =
      pw::thread::zephyr::Options(work_queue_thread_context)
          .set_priority(static_cast<int>(ThreadPriority::kWorkQueue));
  return options;
}

}  // namespace pw::system