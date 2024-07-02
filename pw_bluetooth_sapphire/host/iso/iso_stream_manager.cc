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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream_manager.h"

namespace bt::iso {

IsoStreamManager::IsoStreamManager(hci_spec::ConnectionHandle handle,
                                   hci::CommandChannel::WeakPtr cmd_channel)
    : acl_handle_(handle), cmd_(cmd_channel), weak_self_(this) {
  if (!cmd_.is_alive()) {
    return;
  }
  auto self = GetWeakPtr();
  cis_request_handler_ = cmd_->AddLEMetaEventHandler(
      hci_spec::kLECISRequestSubeventCode,
      [self = std::move(self)](const hci::EmbossEventPacket& event) {
        if (!self.is_alive()) {
          return hci::CommandChannel::EventCallbackResult::kRemove;
        }
        self->OnCisRequest(event);
        return hci::CommandChannel::EventCallbackResult::kContinue;
      });
}

IsoStreamManager::~IsoStreamManager() {
  if (cmd_.is_alive()) {
    cmd_->RemoveEventHandler(cis_request_handler_);
  }
}

void IsoStreamManager::OnCisRequest(const hci::EmbossEventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);

  auto event_view =
      event.view<pw::bluetooth::emboss::LECISRequestSubeventView>();
  BT_ASSERT(event_view.le_meta_event().subevent_code().Read() ==
            hci_spec::kLECISRequestSubeventCode);

  hci_spec::ConnectionHandle request_handle =
      event_view.acl_connection_handle().Read();

  // Ignore any requests that are not intended for this connection.
  if (request_handle != acl_handle_) {
    bt_log(DEBUG,
           "iso",
           "ignoring incoming stream request for handle 0x%x (ours: 0x%x)",
           request_handle,
           acl_handle_);
    return;
  }

  // For now, just reject all incoming requests.
  RejectCisRequest(event_view);
}

void IsoStreamManager::RejectCisRequest(
    const pw::bluetooth::emboss::LECISRequestSubeventView& event_view) {
  hci_spec::ConnectionHandle cis_handle =
      event_view.cis_connection_handle().Read();

  auto command = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LERejectCISRequestCommandWriter>(
      hci_spec::kLERejectCISRequest);
  auto cmd_view = command.view_t();
  cmd_view.connection_handle().Write(cis_handle);
  cmd_view.reason().Write(pw::bluetooth::emboss::StatusCode::UNSPECIFIED_ERROR);

  cmd_->SendCommand(std::move(command),
                    [cis_handle](auto id, const hci::EventPacket& event) {
                      hci_is_error(event,
                                   ERROR,
                                   "bt-iso",
                                   "reject CIS request failed for handle %#x",
                                   cis_handle);
                    });
}

}  // namespace bt::iso
