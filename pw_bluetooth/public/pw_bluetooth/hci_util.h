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

#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_result/result.h"
#include "pw_span/span.h"

namespace pw::bluetooth {

/// Get the size of an Hci Header
///
/// @param type H4 Packet Type
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///   OK: Size of the Hci header
///
///   INVALID_ARGUMENT: An invalid type was provided
///
/// @endrst
pw::Result<size_t> GetHciHeaderSize(emboss::H4PacketType type);

/// Get the size of an Hci Payload
///
/// @param type H4 Packet Type
/// @param hci_header span holding at least a full header
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///   OK: Size of the Hci payload
///
///   INVALID_ARGUMENT: An invalid type was provided
///
///   OUT_OF_RANGE: The span was too small
///
/// @endrst
pw::Result<size_t> GetHciPayloadSize(emboss::H4PacketType type,
                                     pw::span<const uint8_t> hci_header);

}  // namespace pw::bluetooth
