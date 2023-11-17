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
