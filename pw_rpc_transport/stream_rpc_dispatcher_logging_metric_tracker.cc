// Copyright 2025 The Pigweed Authors
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

#define PW_LOG_MODULE_NAME "PW_RPC"

#include "pw_rpc_transport/stream_rpc_dispatcher_logging_metric_tracker.h"

#include <cinttypes>

#include "pw_log/log.h"

namespace pw::rpc {

void StreamRpcDispatcherLoggingMetricTracker::ReadError(Status status) {
  read_error_.Increment();
  PW_LOG_ERROR("StreamRpcDispatcher: read error=%d", status.code());
}

void StreamRpcDispatcherLoggingMetricTracker::EgressError(Status status) {
  egress_error_.Increment();
  PW_LOG_ERROR("StreamRpcDispatcher: egress error=%d", status.code());
}

}  // namespace pw::rpc
