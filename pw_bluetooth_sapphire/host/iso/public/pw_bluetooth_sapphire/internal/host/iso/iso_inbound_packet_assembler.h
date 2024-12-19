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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace bt::iso {

class IsoInboundPacketAssembler {
 public:
  using PacketHandler = pw::Function<void(const pw::span<const std::byte>&)>;
  IsoInboundPacketAssembler(PacketHandler complete_packet_handler)
      : complete_packet_handler_(std::move(complete_packet_handler)) {}

  // Process the next frame received from the transport layer. If it completes a
  // full SDU, pass it along to the complete packet handler.
  void ProcessNext(pw::span<const std::byte> packet);

  // Add a fragment (either an INTERMEDIATE_FRAGMENT or a LAST_FRAGMENT) to
  // assembly_buffer_ and update the buffer headers.
  bool AppendFragment(pw::span<const std::byte> packet);

 private:
  PacketHandler complete_packet_handler_;

  IsoDataPacket assembly_buffer_;
};

}  // namespace bt::iso
