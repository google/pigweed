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

#include "pw_assert/light.h"
#include "pw_router/packet_parser.h"

namespace pw::hdlc {

// HDLC frame parser for routers that operates on wire-encoded frames.
//
// Currently, this assumes 1-byte HDLC address fields. An optional address_bits
// value can be provided to the constructor to use a smaller address size.
class WirePacketParser final : public router::PacketParser {
 public:
  constexpr WirePacketParser(uint8_t address_bits = 8)
      : address_(0), address_shift_(8 - address_bits) {
    PW_ASSERT(address_bits <= 8);
  }

  // Verifies and parses an HDLC frame. Packet passed in is expected to be a
  // single, complete, wire-encoded frame, starting and ending with a flag.
  bool Parse(ConstByteSpan packet) final;

  std::optional<uint32_t> GetDestinationAddress() const final {
    return address_;
  }

 private:
  uint8_t address_;
  uint8_t address_shift_;
};

}  // namespace pw::hdlc
