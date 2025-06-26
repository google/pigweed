
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

#define PW_LOG_MODULE_NAME "PW_RPC"

#include "pw_rpc_transport/egress_ingress_logging_metric_tracker.h"

#include "pw_log/log.h"

namespace pw::rpc {

void RpcIngressLoggingMetricTracker::PacketProcessed(ConstByteSpan /*packet*/) {
  total_packets_.Increment();
}

void RpcIngressLoggingMetricTracker::BadPacket() {
  bad_packets_.Increment();
  PW_LOG_ERROR("Received malformed RPC packet");
}

void RpcIngressLoggingMetricTracker::ChannelIdOverflow(
    uint32_t channel_id, uint32_t max_channel_id) {
  overflow_channel_ids_.Increment();
  PW_LOG_ERROR(
      "Received RPC packet for channel ID %d, max supported channel ID %d",
      static_cast<int>(channel_id),
      static_cast<int>(max_channel_id));
}

void RpcIngressLoggingMetricTracker::MissingEgressForChannel(
    uint32_t channel_id) {
  missing_egresses_.Increment();
  PW_LOG_ERROR(
      "Received RPC packet for channel ID %d"
      " which doesn't have a registered egress",
      static_cast<int>(channel_id));
}

void RpcIngressLoggingMetricTracker::IngressSendFailure(uint32_t channel_id,
                                                        pw::Status status) {
  egress_errors_.Increment();
  PW_LOG_ERROR(
      "Failed to send RPC packet received on channel ID %d"
      " to its configured egress. Status %d",
      static_cast<int>(channel_id),
      static_cast<int>(status.code()));
}

}  // namespace pw::rpc
