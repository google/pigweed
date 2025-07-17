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

namespace pw::multibuf::examples {

// Protocol DemoLink has frames up to 1014 bytes in length with 6-byte headers
// (2 bytes each for src addr, dst addr, and len) and a 4 byte crc32 checksum.
struct DemoLinkHeader {
  uint16_t src_addr;
  uint16_t dst_addr;
  uint16_t length;
};

struct DemoLinkFooter {
  uint32_t crc32;
};

inline constexpr size_t kMaxDemoLinkFrameLength = 1 << 10;

// Protocol DemoNetwork have packets that fit entirely within a DemoLink frame.
// They have 20-byte headers (8 byte src and dst address, and a 4 byte packet
// length).
struct DemoNetworkHeader {
  uint64_t src_addr;
  uint64_t dst_addr;
  uint32_t length;
};

// Protocol DemoTransport has segments up to ~4 GiB spanning multiple packets.
// Each fragment of a segment includes an 12 byte header that includes a
// segment ID, offset and length. The first fragment has and additional 4 byte
// field for the total segment length.
struct DemoTransportHeader {
  uint64_t segment_id;
  uint32_t offset;
  uint32_t length;
};

struct DemoTransportFirstHeader : DemoTransportHeader {
  uint32_t total_length;
};

}  // namespace pw::multibuf::examples
