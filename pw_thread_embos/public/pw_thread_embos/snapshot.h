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

#include "RTOS.h"
#include "pw_protobuf/encoder.h"
#include "pw_status/status.h"
#include "pw_thread/snapshot.h"
#include "pw_thread_protos/thread.pwpb.h"

namespace pw::thread::embos {

// Captures all embos threads in a system as part of a snapshot.
//
// An updated running_thread_stack_pointer must be provided in order for the
// running thread's context to reflect the running state. For ARM, you might do
// something like this:
//
//    // Capture PSP.
//    void* stack_ptr = 0;
//    asm volatile("mrs %0, psp\n" : "=r"(stack_ptr));
//    pw::thread::ProcessThreadStackCallback cb =
//        [](pw::thread::proto::Thread::StreamEncoder& encoder,
//           pw::ConstByteSpan stack) -> pw::Status {
//      return encoder.WriteRawStack(stack);
//    };
//    pw::thread::embos::SnapshotThread(stack_ptr, snapshot_encoder, cb);
//
// Warning: This is only safe to use when interrupts and the scheduler are
// disabled!
Status SnapshotThreads(void* running_thread_stack_pointer,
                       proto::SnapshotThreadInfo::StreamEncoder& encoder,
                       ProcessThreadStackCallback& thread_stack_callback);

// Captures only the provided thread handle as a pw::Thread proto message. After
// thread info capture, the ProcessThreadStackCallback is called to capture
// either the raw_stack or raw_backtrace.
//
// An updated running_thread_stack_pointer must be provided in order for the
// running thread's context to reflect the current state. If the thread being
// captured is not the running thread, the value is ignored. Note that the
// stack pointer in the thread handle is almost always stale on the running
// thread.
//
// Captures the following proto fields:
//   pw.thread.Thread:
//     name (when OS_TRACKNAME is enabled)
//     state
//     stack_start_pointer (when OS_CHECKSTACK or OS_SUPPORT_MPU are enabled)
//     stack_end_pointer (when OS_CHECKSTACK or OS_SUPPORT_MPU are enabled)
//     stack_pointer
//
//
//
//
// Warning: This is only safe to use when interrupts and the scheduler are
// disabled!
Status SnapshotThread(const OS_TASK& thread,
                      void* running_thread_stack_pointer,
                      proto::Thread::StreamEncoder& encoder,
                      ProcessThreadStackCallback& thread_stack_callback);

}  // namespace pw::thread::embos
