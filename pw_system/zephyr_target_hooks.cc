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

#include "pw_thread/attrs.h"
#include "pw_thread/priority.h"
#include "pw_thread/thread.h"
#include "pw_thread_zephyr/context.h"
#include "pw_thread_zephyr/options.h"
#include "pw_thread_zephyr/priority.h"

namespace pw::system {

// Low to high priorities.
constexpr const auto kWorkQueuePriority = pw::ThreadPriority();

// TODO(amontanez): These should ideally be at different priority levels, but
// there's synchronization issues when they are.
constexpr const auto kLogPriority = kWorkQueuePriority;
constexpr const auto kRpcPriority = kWorkQueuePriority;

static constexpr size_t kLogThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_LOG_STACK_SIZE;
static thread::backend::NativeContextWithStack<kLogThreadStackWords>
    log_thread_context;
const thread::Options& LogThreadOptions() {
  static auto options = pw::thread::backend::GetNativeOptions(
      log_thread_context, ThreadAttrs().set_priority(kLogPriority));
  return options;
}

static constexpr size_t kRpcThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_RPC_STACK_SIZE;
static thread::backend::NativeContextWithStack<kRpcThreadStackWords>
    rpc_thread_context;
const thread::Options& RpcThreadOptions() {
  static auto options = pw::thread::backend::GetNativeOptions(
      rpc_thread_context, ThreadAttrs().set_priority(kRpcPriority));
  return options;
}

static constexpr size_t kWorkQueueThreadStackWords =
    CONFIG_PIGWEED_SYSTEM_TARGET_HOOKS_WORK_QUEUE_STACK_SIZE;
static thread::backend::NativeContextWithStack<kWorkQueueThreadStackWords>
    work_queue_thread_context;

const thread::Options& WorkQueueThreadOptions() {
  static auto options = pw::thread::backend::GetNativeOptions(
      work_queue_thread_context,
      ThreadAttrs().set_priority(kWorkQueuePriority));
  return options;
}

}  // namespace pw::system
