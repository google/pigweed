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
#include "pw_thread_freertos/snapshot.h"

#include <span>
#include <string_view>

#include "FreeRTOS.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_protobuf/encoder.h"
#include "pw_status/status.h"
#include "pw_thread/snapshot.h"
#include "pw_thread_freertos/freertos_tsktcb.h"
#include "pw_thread_freertos/util.h"
#include "pw_thread_protos/thread.pwpb.h"
#include "task.h"

namespace pw::thread::freertos {
namespace {

void CaptureThreadState(eTaskState thread_state,
                        Thread::StreamEncoder& encoder) {
  switch (thread_state) {
    case eRunning:
      PW_LOG_INFO("Thread State: RUNNING");
      encoder.WriteState(ThreadState::Enum::RUNNING);
      return;

    case eReady:
      PW_LOG_INFO("Thread State: READY");
      encoder.WriteState(ThreadState::Enum::READY);
      return;

    case eBlocked:
      PW_LOG_INFO("Thread State: BLOCKED");
      encoder.WriteState(ThreadState::Enum::BLOCKED);
      return;

    case eSuspended:
      PW_LOG_INFO("Thread State: SUSPENDED");
      encoder.WriteState(ThreadState::Enum::SUSPENDED);
      return;

    case eDeleted:
      PW_LOG_INFO("Thread State: INACTIVE");
      encoder.WriteState(ThreadState::Enum::INACTIVE);
      return;

    case eInvalid:
    default:
      PW_LOG_INFO("Thread State: UNKNOWN");
      encoder.WriteState(ThreadState::Enum::UNKNOWN);
      return;
  }
}

}  // namespace

Status SnapshotThreads(void* running_thread_stack_pointer,
                       SnapshotThreadInfo::StreamEncoder& encoder,
                       ProcessThreadStackCallback& stack_dumper) {
  struct {
    void* running_thread_stack_pointer;
    SnapshotThreadInfo::StreamEncoder* encoder;
    ProcessThreadStackCallback* stack_dumper;
    Status thread_capture_status;
  } ctx;
  ctx.running_thread_stack_pointer = running_thread_stack_pointer;
  ctx.encoder = &encoder;
  ctx.stack_dumper = &stack_dumper;
  ctx.thread_capture_status = OkStatus();

  ThreadCallback thread_capture_cb(
      [&ctx](TaskHandle_t thread, eTaskState thread_state) -> bool {
        Thread::StreamEncoder thread_encoder = ctx.encoder->GetThreadsEncoder();
        ctx.thread_capture_status.Update(
            SnapshotThread(thread,
                           thread_state,
                           ctx.running_thread_stack_pointer,
                           thread_encoder,
                           *ctx.stack_dumper));
        return true;  // Iterate through all threads.
      });
  if (const Status status = ForEachThread(thread_capture_cb); !status.ok()) {
    PW_LOG_ERROR("Failed to iterate threads during snapshot capture: %d",
                 status.code());
  }
  return ctx.thread_capture_status;
}

Status SnapshotThread(TaskHandle_t thread,
                      eTaskState thread_state,
                      void* running_thread_stack_pointer,
                      Thread::StreamEncoder& encoder,
                      ProcessThreadStackCallback& thread_stack_callback) {
  const tskTCB& tcb = *reinterpret_cast<tskTCB*>(thread);

  PW_LOG_INFO("Capturing thread info for %s", tcb.pcTaskName);
  encoder.WriteName(std::as_bytes(std::span(std::string_view(tcb.pcTaskName))));

  CaptureThreadState(thread_state, encoder);

  // TODO(pwbug/422): Update this once we add support for ascending stacks.
  static_assert(portSTACK_GROWTH < 0, "Ascending stacks are not yet supported");

  // If the thread is active, the stack pointer in the TCB is stale.
  const uintptr_t stack_pointer = reinterpret_cast<uintptr_t>(
      thread_state == eRunning ? running_thread_stack_pointer
                               : tcb.pxTopOfStack);
  const uintptr_t stack_low_addr = reinterpret_cast<uintptr_t>(tcb.pxStack);

#if ((portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1))
  const StackContext thread_ctx = {
      .thread_name = tcb.pcTaskName,
      .stack_low_addr = stack_low_addr,
      .stack_high_addr = reinterpret_cast<uintptr_t>(tcb.pxEndOfStack),
      .stack_pointer = stack_pointer,
  };
  return SnapshotStack(thread_ctx, encoder, thread_stack_callback);
#else
  encoder.WriteStackEndPointer(stack_low_addr);
  encoder.WriteStackPointer(stack_pointer);
  return encoder.status();
#endif  // ((portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1))
}

}  // namespace pw::thread::freertos
