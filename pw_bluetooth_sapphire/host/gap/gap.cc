// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/gap/gap.h"

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

const char* EncryptionStatusToString(
    pw::bluetooth::emboss::EncryptionStatus status) {
  switch (status) {
    case pw::bluetooth::emboss::EncryptionStatus::OFF:
      return "OFF";
    case pw::bluetooth::emboss::EncryptionStatus::
        ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE:
      return "ON WITH E0 FOR BR/EDR OR AES FOR LE";
    case pw::bluetooth::emboss::EncryptionStatus::ON_WITH_AES_FOR_BREDR:
      return "ON WITH AES-CCM FOR BR/EDR";
  }

  return "(Unknown)";
}

}  // namespace bt::gap
