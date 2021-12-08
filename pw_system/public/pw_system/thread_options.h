// Copyright 2021 The Pigweed Authors
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

#include "FreeRTOS.h"
#include "pw_thread_freertos/context.h"
#include "pw_thread_freertos/options.h"

namespace pw::system {

// Low to high priorities
enum ThreadPriority : UBaseType_t {
  kIdleQueue = tskIDLE_PRIORITY + 1,
  kLog = kIdleQueue,
  kRpc = kIdleQueue,
  kNumPriorities,
};
static_assert(ThreadPriority::kNumPriorities <= configMAX_PRIORITIES);

static constexpr size_t kLogThreadStackWorkds = 1024;
thread::freertos::StaticContextWithStack<kLogThreadStackWorkds>
    log_thread_context;
const thread::Options& LogThreadOptions() {
  static constexpr auto options = pw::thread::freertos::Options()
                                      .set_name("LogThread")
                                      .set_static_context(log_thread_context)
                                      .set_priority(ThreadPriority::kLog);
  return options;
}

static constexpr size_t kRpcThreadStackWorkds = 512;
thread::freertos::StaticContextWithStack<kRpcThreadStackWorkds>
    rpc_thread_context;
const thread::Options& RpcThreadOptions() {
  static constexpr auto options = pw::thread::freertos::Options()
                                      .set_name("RpcThread")
                                      .set_static_context(rpc_thread_context)
                                      .set_priority(ThreadPriority::kRpc);
  return options;
}
}  // namespace pw::system
