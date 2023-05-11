// Copyright 2020 The Pigweed Authors
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

#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::hdlc {

/// @brief Writes an HDLC unnumbered information frame (UI frame) to the
/// provided `pw::stream` writer.
///
/// @param address The frame address.
///
/// @param payload The frame data to encode.
///
/// @param writer The `pw::stream` to write the frame to. The frame contains
/// the following bytes. See [pw_hdlc: Design](/pw_hdlc/design.html) for more
/// information.
/// * HDLC flag byte (`0x7e`)
/// * Address (variable length, up to 10 bytes)
/// * UI-frame control (metadata) byte
/// * Payload (0 or more bytes)
/// * Frame check sequence (CRC-32, 4 bytes)
/// * HDLC flag byte (`0x7e`)
///
/// @returns A [pw::Status](/pw_status/) instance describing the result of the
/// operation:
/// * `pw::Status::Ok()`: The write finished successfully.
/// * `pw::Status::ResourceExhausted()`: The write failed because the size of
///   the frame would be larger than the writer's conservative limit.
/// * `pw::Status::InvalidArgument()`: The start of the write failed. Check
///   for problems in your `address` argument's value.
Status WriteUIFrame(uint64_t address,
                    ConstByteSpan payload,
                    stream::Writer& writer);

}  // namespace pw::hdlc
