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
//

#include "pw_bluetooth_sapphire/internal/host/iso/iso_inbound_packet_assembler.h"

#include <pw_bluetooth/hci_data.emb.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

namespace bt::iso {

void IsoInboundPacketAssembler::ProcessNext(pw::span<const std::byte> packet) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(packet.data());
  auto header_view =
      pw::bluetooth::emboss::MakeIsoDataFrameHeaderView(data, packet.size());

  switch (header_view.pb_flag().Read()) {
    case pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU:
      BT_ASSERT(complete_packet_handler_);
      complete_packet_handler_(packet);
      break;
    case pw::bluetooth::emboss::IsoDataPbFlag::FIRST_FRAGMENT:
    case pw::bluetooth::emboss::IsoDataPbFlag::INTERMEDIATE_FRAGMENT:
    case pw::bluetooth::emboss::IsoDataPbFlag::LAST_FRAGMENT:
      // TODO(b/311639690): Add support for fragmented SDUs
      bt_log(ERROR,
             "iso",
             "ISO packet fragmentation not supported yet - dropping packet");
      break;
  }
}

}  // namespace bt::iso
