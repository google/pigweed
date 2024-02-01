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

#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_string/string.h"

namespace pw::grpc {

// We disable the HPACK dynamic header table.
inline constexpr uint32_t kHpackDynamicHeaderTableSize = 0;

// Maximum size of a string that can be returned by this API.
inline constexpr uint32_t kHpackMaxStringSize = 127;

// Parses a request header field block, returning the grpc method name.
Result<InlineString<kHpackMaxStringSize>> HpackParseRequestHeaders(
    ConstByteSpan payload);

// Decodes an HPACK integer.
// Consumed bytes are removed from the `input` span.
Result<int> HpackIntegerDecode(ConstByteSpan& input, int bits_in_first_byte);

// Decodes an HPACK string.
// Consumed bytes are removed from the `input` span.
Result<InlineString<kHpackMaxStringSize>> HpackStringDecode(
    ConstByteSpan& input);

// Decodes a Huffman-encoded string.
Result<InlineString<kHpackMaxStringSize>> HpackHuffmanDecode(
    ConstByteSpan input);

// Returns a HEADERS payload to use for grpc Response-Headers.
ConstByteSpan ResponseHeadersPayload();

// Returns a HEADERS payload to use for grpc Trailers.
ConstByteSpan ResponseTrailersPayload(Status response_code);

}  // namespace pw::grpc
