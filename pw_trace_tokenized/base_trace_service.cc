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

#include "pw_trace_tokenized/base_trace_service.h"

#include "pw_log/log.h"
#include "pw_trace_tokenized/trace_buffer.h"

namespace pw::trace {
BaseTraceService::BaseTraceService(TokenizedTracer& tokenized_tracer,
                                   stream::Writer& trace_writer)
    : tokenized_tracer_(tokenized_tracer), trace_writer_(trace_writer) {
  tokenized_tracer_.Enable(false);
}

Status BaseTraceService::Start() {
  PW_LOG_INFO("Starting Tracing");

  if (tokenized_tracer_.IsEnabled()) {
    PW_LOG_INFO("Tracing already started");
    return Status::FailedPrecondition();
  }

  tokenized_tracer_.Enable(true);

  return OkStatus();
}

Status BaseTraceService::Stop() {
  PW_LOG_INFO("Stopping Tracing");

  if (!tokenized_tracer_.IsEnabled()) {
    PW_LOG_INFO("Tracing not started");
    return Status::FailedPrecondition();
  }

  tokenized_tracer_.Enable(false);

  auto ring_buffer = trace::GetBuffer();

  if (ring_buffer->EntryCount() == 0) {
    PW_LOG_WARN("EntryCount(%zu)", ring_buffer->EntryCount());
    return Status::Unavailable();
  } else if (ring_buffer->CheckForCorruption() != OkStatus()) {
    PW_LOG_ERROR("EntryCount(%zu), Corruption(%d)",
                 ring_buffer->EntryCount(),
                 ring_buffer->CheckForCorruption().code());
    return Status::Unavailable();
  }

  PW_LOG_INFO("EntryCount(%zu)", ring_buffer->EntryCount());

  ConstByteSpan inner_trace_span = trace::DeringAndViewRawBuffer();
  if (auto status = trace_writer_.Write(inner_trace_span);
      status != OkStatus()) {
    PW_LOG_ERROR("Failed to write trace data: %d)", status.code());
    return status;
  }

  ring_buffer->Clear();

  return OkStatus();
}

}  // namespace pw::trace
