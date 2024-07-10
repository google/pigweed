// Copyright 2024 The Pigweed Authors
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

#include <pw_bluetooth/hci_data.emb.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"

namespace bt::iso {

// Maximum possible size of an Isochronous data packet.
// See Core Spec v5.4, Volume 4, Part E, Section 5.4.5
constexpr size_t kMaxIsochronousDataPacketSize =
    pw::bluetooth::emboss::IsoDataFrameHeader::MaxSizeInBytes() +
    hci_spec::kMaxIsochronousDataPacketPayloadSize;
}  // namespace bt::iso
