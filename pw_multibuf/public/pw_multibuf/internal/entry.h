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

#include <cstddef>
#include <cstdint>

namespace pw::multibuf::internal {

/// Describes either a memory location or a view of an associated location.
///
/// This module stores byte buffers in queues using sequences of entries. The
/// first entry holds an address, and subsequent entries hold the offset and
/// lengths of ever-narrower views of that data. This provides a compact
/// representation of data encoded using nested or layered protocols.
///
/// For example, in a TCP/IP stack:
///  * The first entry hold addresses to Ethernet frames.
///  * The second entry holds a zero offset and the whole frame length.
///  * The third entry holds the offset and length describing the IP data.
///  * The fourth entry holds the offset and length describing the TCP data.
///
/// The boundary flag is set when adding an entry or consolidating several
/// entries in a new layer. It is used to determine how many entries represent
/// a packet or message fragment at a particular protocol layer.
union Entry {
  std::byte* data;
  struct View {
    uint16_t offset;
    uint16_t length : 15;
    uint16_t boundary : 1;
  } view;
};

}  // namespace pw::multibuf::internal
