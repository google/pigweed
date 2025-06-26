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

#include "pw_rpc_transport/local_rpc_egress_logging_metric_tracker.h"

#define PW_LOG_MODULE_NAME "PW_RPC"

#include "pw_log/log.h"

namespace pw::rpc {

void LocalRpcEgressLoggingMetricTracker::NoRpcServiceRegistryError() {
  PW_LOG_ERROR("LocalRpcEgress: service registry not configured");
}

void LocalRpcEgressLoggingMetricTracker::PacketSizeTooLarge(
    size_t packet_size, size_t max_packet_size) {
  packet_size_too_large_.Increment();
  PW_LOG_ERROR("LocalRpcEgress: packet too large (%d > %d)",
               static_cast<int>(packet_size),
               static_cast<int>(max_packet_size));
}

void LocalRpcEgressLoggingMetricTracker::EgressThreadNotRunningError() {
  PW_LOG_ERROR("LocalRpcEgress: egress thread is not running");
}

void LocalRpcEgressLoggingMetricTracker::FailedToProcessPacket(Status status) {
  failed_to_process_packet_.Increment();
  PW_LOG_ERROR("LocalRpcEgress: failed to process packet. Status %d",
               static_cast<int>(status.code()));
}

void LocalRpcEgressLoggingMetricTracker::FailedToAccessPacket(Status status) {
  failed_to_access_packet_.Increment();
  PW_LOG_ERROR("LocalRpcEgress: failed to access packet buffer. Status %d",
               static_cast<int>(status.code()));
}

void LocalRpcEgressLoggingMetricTracker::NoPacketAvailable(Status status) {
  no_packet_available_.Increment();
  PW_LOG_ERROR("LocalRpcEgress: no packet available. Status %d",
               static_cast<int>(status.code()));
}

void LocalRpcEgressLoggingMetricTracker::PacketProcessed(
    [[maybe_unused]] ConstByteSpan packet,
    [[maybe_unused]] chrono::SystemClock::duration processing_duration) {}

}  // namespace pw::rpc
