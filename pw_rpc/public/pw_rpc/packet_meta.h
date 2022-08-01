// Copyright 2022 The Pigweed Authors
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

#include "pw_rpc/internal/packet.h"
#include "pw_rpc/service_id.h"
#include "pw_span/span.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc {

// Metadata about a `pw_rpc` packet.
//
// For now, this metadata structure only includes a limited set of information
// about the contents of a packet, but it may be extended in the future.
class PacketMeta {
 public:
  // Parses the metadata from a serialized packet.
  static Result<PacketMeta> FromBuffer(ConstByteSpan data);
  constexpr uint32_t channel_id() const { return channel_id_; }
  constexpr ServiceId service_id() const { return service_id_; }

 private:
  constexpr explicit PacketMeta(const internal::Packet packet)
      : channel_id_(packet.channel_id()),
        service_id_(internal::WrapServiceId(packet.service_id())) {}
  uint32_t channel_id_;
  ServiceId service_id_;
};

}  // namespace pw::rpc