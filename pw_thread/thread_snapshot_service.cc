// Copyright 2022 The Pigweed Authors
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

#include "pw_thread/thread_snapshot_service.h"

#include "pw_log/log.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_thread/thread_info.h"
#include "pw_thread/thread_iteration.h"
#include "pw_thread_private/thread_snapshot_service.h"
#include "pw_thread_protos/thread.pwpb.h"
#include "pw_thread_protos/thread_snapshot_service.pwpb.h"

namespace pw::thread {

Status ProtoEncodeThreadInfo(SnapshotThreadInfo::MemoryEncoder& encoder,
                             const ThreadInfo& thread_info) {
  // Grab the next available Thread slot to write to in the response.
  Thread::StreamEncoder proto_encoder = encoder.GetThreadsEncoder();
  if (thread_info.thread_name().has_value()) {
    PW_TRY(proto_encoder.WriteName(thread_info.thread_name().value()));
  } else {
    // Name is necessary to identify thread.
    return Status::FailedPrecondition();
  }
  if (thread_info.stack_low_addr().has_value()) {
    PW_TRY(proto_encoder.WriteStackEndPointer(
        thread_info.stack_low_addr().value()));
  }
  if (thread_info.stack_high_addr().has_value()) {
    PW_TRY(proto_encoder.WriteStackStartPointer(
        thread_info.stack_high_addr().value()));
  } else {
    // Need stack start pointer to contextualize estimated peak.
    return Status::FailedPrecondition();
  }
  if (thread_info.stack_peak_addr().has_value()) {
    PW_TRY(proto_encoder.WriteStackPointerEstPeak(
        thread_info.stack_peak_addr().value()));
  } else {
    // Peak stack usage reporting is not supported.
    return Status::Unimplemented();
  }

  return proto_encoder.status();
}

void ErrorLog(Status status) {
  if (status == Status::Unimplemented()) {
    PW_LOG_ERROR(
        "Peak stack usage reporting not supported by your current OS or "
        "configuration.");
  } else if (status == Status::FailedPrecondition()) {
    PW_LOG_ERROR("Thread missing information needed by service.");
  } else if (status == Status::ResourceExhausted()) {
    PW_LOG_ERROR("Buffer capacity limit exceeded.");
  } else if (status != OkStatus()) {
    PW_LOG_ERROR("RPC service was unable to capture thread information");
  }
}

void ThreadSnapshotService::GetPeakStackUsage(
    ConstByteSpan /* request */, rpc::RawServerWriter& response_writer) {
  // For now, ignore the request and just stream all the thread information
  // back.
  struct IterationInfo {
    SnapshotThreadInfo::MemoryEncoder encoder;
    Status status;
  };

  IterationInfo iteration_info{
      SnapshotThreadInfo::MemoryEncoder(encode_buffer_), OkStatus()};

  auto cb = [&iteration_info](const ThreadInfo& thread_info) {
    iteration_info.status.Update(
        ProtoEncodeThreadInfo(iteration_info.encoder, thread_info));
    return iteration_info.status.ok();
  };
  ForEachThread(cb);

  // This logging action is external to thread iteration because it is
  // unsafe to log within ForEachThread() when the scheduler is disabled.
  ErrorLog(iteration_info.status);

  Status status;
  if (iteration_info.encoder.size() && iteration_info.status.ok()) {
    status = response_writer.Write(iteration_info.encoder);
  }
  if (response_writer.Finish(status) != OkStatus()) {
    PW_LOG_ERROR(
        "Failed to close stream for GetPeakStackUsage() with error code %d",
        status.code());
  }
}

}  // namespace pw::thread
