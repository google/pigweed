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

#include "pw_bluetooth_proxy/comms2/l2cap_task.h"

#include "pw_allocator/allocator.h"
#include "pw_async2/try.h"
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth_proxy/comms2/proxy.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

L2capTask::L2capTask(Direction direction,
                     channel::PacketReader<H4Packet>& reader,
                     channel::PacketWriter<H4Packet>& writer)
    : Base(reader, writer, queue_), direction_(direction) {}

void L2capTask::HandlePacket(H4Packet&& packet) {
  if (!MaybeHandlePacket(packet)) {
    // Not handled; queue for output.
    queue_.push(std::move(packet));
  }
}

bool L2capTask::MaybeHandlePacket(H4Packet& h4_packet) {
  switch (h4_packet.type()) {
    case H4Packet::Type::COMMAND:
      return HandleHciCommand(h4_packet);
    case H4Packet::Type::EVENT:
      return HandleHciEvent(h4_packet);
    case H4Packet::Type::ACL_DATA:
      return HandleAclData(h4_packet);
    case H4Packet::Type::UNKNOWN:
    case H4Packet::Type::SYNC_DATA:
    case H4Packet::Type::ISO_DATA:
    default:
      return false;
  }
}

bool L2capTask::HandleHciCommand(H4Packet& h4_packet) {
  if (direction() == Direction::kFromController) {
    return false;  // Ignore commands from the controller.
  }
  emboss::OpCode opcode = emboss::OpCode::UNSPECIFIED;
  auto status = h4_packet.Visit<emboss::CommandHeaderView,
                                emboss::CommandHeader::IntrinsicSizeInBytes()>(
      [&opcode](auto header) { opcode = header.opcode().Read(); });
  if (status.ok() && opcode == emboss::OpCode::RESET) {
    PW_LOG_INFO("Resetting proxy on HCI_Reset Command from host.");
    RequestReset();
    return true;
  }
  // Ignore all other unrecognized commands.
  return false;
}

bool L2capTask::HandleHciEvent(H4Packet&) {
  // TODO: b/430175134 - Implement HCI event handling
  return false;
}

bool L2capTask::HandleAclData(H4Packet&) {
  // TODO: b/430175134 - Implement ACL data handling
  return false;
}

}  // namespace pw::bluetooth::proxy
