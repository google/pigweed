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
#include "pw_rpc_transport/stream_rpc_dispatcher.h"

namespace pw::rpc {

class StreamRpcDispatcherLoggingMetricTracker final
    : public StreamRpcDispatcherTracker {
 public:
  void ReadError(Status status) override;
  void EgressError(Status status) override;

  pw::metric::Group& metrics() { return metrics_; }
  const pw::metric::Group& metrics() const { return metrics_; }

  uint32_t read_errors() const { return read_error_.value(); }
  uint32_t egress_errors() const { return egress_error_.value(); }

 private:
  PW_METRIC_GROUP(metrics_, "stream_rpc_dispatcher");
  PW_METRIC(metrics_, read_error_, "read_error", 0);
  PW_METRIC(metrics_, egress_error_, "egress_error", 0);
};

}  // namespace pw::rpc
