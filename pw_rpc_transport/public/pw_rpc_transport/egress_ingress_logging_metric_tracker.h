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
#pragma once

#include <cstdint>

#include "pw_metric/metric.h"
#include "pw_rpc_transport/egress_ingress.h"

namespace pw::rpc {

class RpcIngressLoggingMetricTracker : public RpcIngressTracker {
 public:
  void PacketProcessed(ConstByteSpan packet) override;
  void BadPacket() override;
  void ChannelIdOverflow(uint32_t channel_id, uint32_t max_channel_id) override;
  void MissingEgressForChannel(uint32_t channel_id) override;
  void IngressSendFailure(uint32_t channel_id, Status status) override;

  metric::Group& metrics() { return metrics_; }
  const metric::Group& metrics() const { return metrics_; }

  uint32_t total_packets() const { return total_packets_.value(); }
  uint32_t bad_packets() const { return bad_packets_.value(); }
  uint32_t overflow_channel_ids() const {
    return overflow_channel_ids_.value();
  }
  uint32_t missing_egresses() const { return missing_egresses_.value(); }
  uint32_t egress_errors() const { return egress_errors_.value(); }

 private:
  PW_METRIC_GROUP(metrics_, "rpc_ingress");
  PW_METRIC(metrics_, total_packets_, "total_packets", 0u);
  PW_METRIC(metrics_, bad_packets_, "bad_packets", 0u);
  PW_METRIC(metrics_, overflow_channel_ids_, "overflow_channel_ids", 0u);
  PW_METRIC(metrics_, missing_egresses_, "missing_egresses", 0u);
  PW_METRIC(metrics_, egress_errors_, "egress_errors", 0u);
};

}  // namespace pw::rpc
