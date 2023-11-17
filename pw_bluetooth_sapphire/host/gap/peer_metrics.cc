
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

#include "pw_bluetooth_sapphire/internal/host/gap/peer_metrics.h"

namespace bt::gap {

void PeerMetrics::AttachInspect(inspect::Node& parent) {
  metrics_node_ = parent.CreateChild(kInspectNodeName);

  metrics_le_node_ = metrics_node_.CreateChild("le");
  le_bond_success_.AttachInspect(metrics_le_node_, "bond_success_events");
  le_bond_failure_.AttachInspect(metrics_le_node_, "bond_failure_events");
  le_connections_.AttachInspect(metrics_le_node_, "connection_events");
  le_disconnections_.AttachInspect(metrics_le_node_, "disconnection_events");

  metrics_bredr_node_ = metrics_node_.CreateChild("bredr");
  bredr_bond_success_.AttachInspect(metrics_bredr_node_, "bond_success_events");
  bredr_bond_failure_.AttachInspect(metrics_bredr_node_, "bond_failure_events");
  bredr_connections_.AttachInspect(metrics_bredr_node_, "connection_events");
  bredr_disconnections_.AttachInspect(metrics_bredr_node_,
                                      "disconnection_events");
}

}  // namespace bt::gap
