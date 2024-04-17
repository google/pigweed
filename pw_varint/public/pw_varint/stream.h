// Copyright 2021 The Pigweed Authors
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

#include <cstdint>
#include <limits>

#include "pw_status/status_with_size.h"
#include "pw_stream/stream.h"

namespace pw {
namespace varint {

/// @brief Decodes a variable-length integer (varint) from the current position
/// of a `pw::stream`. Reads a maximum of 10 bytes or `max_size`, whichever is
/// smaller.
///
/// This method always returns the number of bytes read, even on error.
///
/// @param reader The `pw::stream` to read from.
///
/// @param output The integer to read into. If reading into a signed integer,
/// the value is
/// [ZigZag](https://protobuf.dev/programming-guides/encoding/#signed-ints)-decoded.
///
/// @param max_size The maximum number of bytes to read. The upper limit is 10
/// bytes.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: The decoded value is placed in ``output``.
///
///    OUT_OF_RANGE: No input is available, e.g. the stream is closed.
///
///    DATA_LOSS: The decoded value is too large for ``output`` or is
///    incomplete, e.g. the input was exhausted after a partial varint was
///    read.
///
/// @endrst
StatusWithSize Read(stream::Reader& reader,
                    int64_t* output,
                    size_t max_size = std::numeric_limits<size_t>::max());
StatusWithSize Read(stream::Reader& reader,
                    uint64_t* output,
                    size_t max_size = std::numeric_limits<size_t>::max());

}  // namespace varint
}  // namespace pw
