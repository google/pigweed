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

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
namespace bt::hci_spec {

// Represents a received BIGInfo report.
struct BroadcastIsochronousGroupInfo {
  uint8_t num_bis;
  uint8_t nse;
  uint16_t iso_interval;
  uint8_t bn;
  uint8_t pto;
  uint8_t irc;
  uint16_t max_pdu;
  uint32_t sdu_interval;
  uint16_t max_sdu;
  pw::bluetooth::emboss::IsoPhyType phy;
  pw::bluetooth::emboss::BigFraming framing;
  bool encryption;
};
}  // namespace bt::hci_spec
