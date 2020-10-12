// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter_state.h"

namespace bt {
namespace gap {

AdapterState::AdapterState() : vendor_features_(0u) {
  std::memset(supported_commands_, 0, sizeof(supported_commands_));
}

}  // namespace gap
}  // namespace bt
