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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/common/metrics.h"

namespace bt::gap {

// Represents the shared metric counters updated across all peers
class PeerMetrics {
 public:
  static constexpr const char* kInspectNodeName = "metrics";

  PeerMetrics() = default;

  // Attach metrics node to |parent| peer cache inspect node.
  void AttachInspect(inspect::Node& parent);

  // Log LE bonding success.
  void LogLeBondSuccessEvent() { le_bond_success_.Add(); }

  // Log LE bonding failure.
  void LogLeBondFailureEvent() { le_bond_failure_.Add(); }

  // Log LE connection event.
  void LogLeConnection() { le_connections_.Add(); }

  // Log LE disconnection event.
  void LogLeDisconnection() { le_disconnections_.Add(); }

  // Log BrEdr bonding success.
  void LogBrEdrBondSuccessEvent() { bredr_bond_success_.Add(); }

  // Log BrEdr bonding failure.
  void LogBrEdrBondFailureEvent() { bredr_bond_failure_.Add(); }

  // Log BrEdr connection event.
  void LogBrEdrConnection() { bredr_connections_.Add(); }

  // Log BrEdr disconnection event.
  void LogBrEdrDisconnection() { bredr_disconnections_.Add(); }

 private:
  inspect::Node metrics_node_;
  inspect::Node metrics_le_node_;
  inspect::Node metrics_bredr_node_;

  UintMetricCounter le_bond_success_;
  UintMetricCounter le_bond_failure_;
  UintMetricCounter le_connections_;
  UintMetricCounter le_disconnections_;
  UintMetricCounter bredr_bond_success_;
  UintMetricCounter bredr_bond_failure_;
  UintMetricCounter bredr_connections_;
  UintMetricCounter bredr_disconnections_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PeerMetrics);
};

}  // namespace bt::gap
