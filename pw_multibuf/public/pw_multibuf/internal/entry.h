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
#include <limits>

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
union Entry {
  /// Entries fit in a single word on 32-bit platforms and larger. Fields are
  /// ordered in such a way to ensure this is true on supported platforms.
  using size_type = uint16_t;

  /// Offset and length must fit in 15 bits.
  static constexpr size_t kMaxSize = ~(1U << 15);

  /// Pointer to memory.
  std::byte* data;

  /// The first entry after the data
  struct BaseView {
    /// Starting offset within the buffer of the data to present.
    size_type offset : 15;

    /// Indicates this memory is "owned", i.e. it should be deallocated when the
    /// entry goes out of scope.
    size_type owned : 1;

    /// Amount of data from the buffer to present.
    size_type length : 15;

    /// Indicates this memory is "shared", i.e. there may be other references to
    /// it.
    size_type shared : 1;
  } base_view;

  /// Each of the `depth - 2` subsequent entries describe the view of that data
  /// that makes up part of a MultiBuf "layer".
  struct View {
    /// Starting offset within the buffer of the data to present.
    size_type offset : 15;

    /// Flag that is set when a layer should not be modified or removed. This
    /// can be used by lower levels of a protocol stack to indicate that upper
    /// or application layers should not modify data. This is informational and
    /// bypassable, and so should not be considered a security mechanism.
    size_type sealed : 1;

    /// Amount of data from the buffer to present.
    size_type length : 15;

    /// Flag that is set when adding an entry or consolidating several entries
    /// in a new layer. It is used to determine how many entries represent a
    /// packet or message fragment at a particular protocol layer.
    size_type boundary : 1;
  } view;
};

}  // namespace pw::multibuf::internal
