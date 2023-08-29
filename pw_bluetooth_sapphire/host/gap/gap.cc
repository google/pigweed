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

const char* BrEdrSecurityModeToString(BrEdrSecurityMode mode) {
  switch (mode) {
    case BrEdrSecurityMode::Mode4:
      return "Mode 4";
    case BrEdrSecurityMode::SecureConnectionsOnly:
      return "Secure Connections Only Mode";
  }
}

const char* LeSecurityModeToString(LESecurityMode mode) {
  switch (mode) {
    case LESecurityMode::Mode1:
      return "Mode 1";
    case LESecurityMode::SecureConnectionsOnly:
      return "Secure Connections Only Mode";
  }
}

}  // namespace bt::gap
