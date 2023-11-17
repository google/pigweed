
// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
