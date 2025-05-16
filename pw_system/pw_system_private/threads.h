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
#pragma once

#include "pw_system/config.h"
#include "pw_thread/attrs.h"

namespace pw::system {

// TODO(amontanez): These should ideally be at different priority levels, but
// there's synchronization issues when they are.
inline constexpr ThreadPriority kThreadPriorityLog = ThreadPriority();
inline constexpr ThreadPriority kThreadPriorityRpc = ThreadPriority();
inline constexpr ThreadPriority kThreadPriorityTransfer = ThreadPriority();
inline constexpr ThreadPriority kThreadPriorityDispatcher = ThreadPriority();
inline constexpr ThreadPriority kThreadPriorityWorkQueue = ThreadPriority();

inline constexpr pw::ThreadAttrs kLogThread =
    pw::ThreadAttrs()
        .set_stack_size_bytes(kLogThreadStackSizeBytes)
        .set_name("LogThread")
        .set_priority(kThreadPriorityLog);

inline constexpr pw::ThreadAttrs kRpcThread =
    pw::ThreadAttrs()
        .set_stack_size_bytes(kRpcThreadStackSizeBytes)
        .set_name("RpcThread")
        .set_priority(kThreadPriorityRpc);

inline constexpr pw::ThreadAttrs kTransferThread =
    pw::ThreadAttrs()
        .set_stack_size_bytes(kTransferThreadStackSizeBytes)
        .set_name("TransferThread")
        .set_priority(kThreadPriorityTransfer);

inline constexpr pw::ThreadAttrs kDispatcherThread =
    pw::ThreadAttrs()
        .set_stack_size_bytes(kDispatcherThreadStackSizeBytes)
        .set_name("DispatcherThread")
        .set_priority(kThreadPriorityDispatcher);

inline constexpr pw::ThreadAttrs kWorkQueueThread =
    pw::ThreadAttrs()
        .set_stack_size_bytes(kWorkQueueThreadStackSizeBytes)
        .set_name("WorkQueueThread")
        .set_priority(kThreadPriorityWorkQueue);

[[noreturn]] void StartScheduler();

}  // namespace pw::system
