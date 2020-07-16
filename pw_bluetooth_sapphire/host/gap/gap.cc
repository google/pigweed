// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gap.h"

namespace bt::gap {

const char* TechnologyTypeToString(TechnologyType type) {
  switch (type) {
    case TechnologyType::kClassic:
      return "Classic";
    case TechnologyType::kDualMode:
      return "DualMode";
    case TechnologyType::kLowEnergy:
      return "LowEnergy";
  }
}

const char* LeSecurityModeToString(LeSecurityMode mode) {
  switch (mode) {
    case LeSecurityMode::Mode1:
      return "Mode 1";
    case LeSecurityMode::SecureConnectionsOnly:
      return "Secure Connections Only Mode";
  }
}

}  // namespace bt::gap
