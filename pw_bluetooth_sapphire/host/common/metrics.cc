// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/common/metrics.h"

namespace bt {

template <>
void IntMetricCounter::AttachInspect(inspect::Node& parent,
                                     const std::string& name) {
  inspect_property_ = parent.CreateInt(name, 0);
}

template <>
void UintMetricCounter::AttachInspect(inspect::Node& parent,
                                      const std::string& name) {
  inspect_property_ = parent.CreateUint(name, 0);
}

}  // namespace bt
