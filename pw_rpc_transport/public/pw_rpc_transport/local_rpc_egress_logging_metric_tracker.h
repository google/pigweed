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

#include <cstddef>

#include "pw_metric/metric.h"
#include "pw_rpc_transport/local_rpc_egress.h"

namespace pw::rpc {

class LocalRpcEgressLoggingMetricTracker final : public LocalRpcEgressTracker {
 public:
  void NoRpcServiceRegistryError() override;
  void PacketSizeTooLarge(size_t packet_size, size_t max_packet_size) override;
  void EgressThreadNotRunningError() override;
  void FailedToProcessPacket(Status status) override;
  void FailedToAccessPacket(Status status) override;
  void NoPacketAvailable(Status status) override;
  void PacketProcessed(
      ConstByteSpan packet,
      chrono::SystemClock::duration processing_duration) override;

  pw::metric::Group& metrics() { return metrics_; }
  const pw::metric::Group& metrics() const { return metrics_; }

 private:
  PW_METRIC_GROUP(metrics_, "local_egress");
  PW_METRIC(metrics_, packet_size_too_large_, "packet_size_too_large", 0);
  PW_METRIC(metrics_, no_packet_available_, "no_packet_available", 0);
  PW_METRIC(metrics_, failed_to_process_packet_, "failed_to_process_packet", 0);
  PW_METRIC(metrics_, failed_to_access_packet_, "failed_to_access_packet", 0);
};

}  // namespace pw::rpc
