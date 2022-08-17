// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection.h"

#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

LowEnergyConnection::LowEnergyConnection(hci_spec::ConnectionHandle handle,
                                         const DeviceAddress& local_address,
                                         const DeviceAddress& peer_address,
                                         const hci_spec::LEConnectionParameters& params,
                                         hci_spec::ConnectionRole role,
                                         const fxl::WeakPtr<Transport>& hci)
    : AclConnection(handle, local_address, peer_address, role, hci),
      parameters_(params),
      weak_ptr_factory_(this) {
  BT_ASSERT(local_address.type() != DeviceAddress::Type::kBREDR);
  BT_ASSERT(peer_address.type() != DeviceAddress::Type::kBREDR);

  le_ltk_request_id_ = hci->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLELongTermKeyRequestSubeventCode,
      fit::bind_member<&LowEnergyConnection::OnLELongTermKeyRequestEvent>(this));

  // Allow packets to be sent on this link immediately.
  hci->acl_data_channel()->RegisterLink(handle, bt::LinkType::kLE);
}

LowEnergyConnection::~LowEnergyConnection() {
  // Unregister HCI event handlers.
  hci()->command_channel()->RemoveEventHandler(le_ltk_request_id_);
}

bool LowEnergyConnection::StartEncryption() {
  if (state() != Connection::State::kConnected) {
    bt_log(DEBUG, "hci", "connection closed; cannot start encryption");
    return false;
  }
  if (role() != hci_spec::ConnectionRole::kCentral) {
    bt_log(DEBUG, "hci", "only the central can start encryption");
    return false;
  }
  if (!ltk().has_value()) {
    bt_log(DEBUG, "hci", "connection has no LTK; cannot start encryption");
    return false;
  }

  auto cmd = CommandPacket::New(hci_spec::kLEStartEncryption,
                                sizeof(hci_spec::LEStartEncryptionCommandParams));
  auto* params = cmd->mutable_payload<hci_spec::LEStartEncryptionCommandParams>();
  params->connection_handle = htole16(handle());
  params->random_number = htole64(ltk()->rand());
  params->encrypted_diversifier = htole16(ltk()->ediv());
  params->long_term_key = ltk()->value();

  auto event_cb = [self = weak_ptr_factory_.GetWeakPtr(), handle = handle()](
                      auto id, const EventPacket& event) {
    if (!self) {
      return;
    }

    Result<> result = event.ToResult();
    if (bt_is_error(result, ERROR, "hci-le", "could not set encryption on link %#.04x", handle)) {
      if (self->encryption_change_callback()) {
        self->encryption_change_callback()(result.take_error());
      }
      return;
    }
    bt_log(DEBUG, "hci-le", "requested encryption start on %#.04x", handle);
  };
  return hci()->command_channel()->SendCommand(std::move(cmd), std::move(event_cb),
                                               hci_spec::kCommandStatusEventCode);
}

void LowEnergyConnection::HandleEncryptionStatus(Result<bool> result, bool /*key_refreshed*/) {
  // "On an authentication failure, the connection shall be automatically
  // disconnected by the Link Layer." (HCI_LE_Start_Encryption, Vol 2, Part E,
  // 7.8.24). We make sure of this by telling the controller to disconnect.
  if (result.is_error()) {
    Disconnect(hci_spec::StatusCode::kAuthenticationFailure);
  }

  if (!encryption_change_callback()) {
    bt_log(DEBUG, "hci", "%#.4x: no encryption status callback assigned", handle());
    return;
  }
  encryption_change_callback()(result);
}

CommandChannel::EventCallbackResult LowEnergyConnection::OnLELongTermKeyRequestEvent(
    const EventPacket& event) {
  BT_ASSERT(event.event_code() == hci_spec::kLEMetaEventCode);
  BT_ASSERT(event.params<hci_spec::LEMetaEventParams>().subevent_code ==
            hci_spec::kLELongTermKeyRequestSubeventCode);

  auto* params = event.subevent_params<hci_spec::LELongTermKeyRequestSubeventParams>();
  if (!params) {
    bt_log(WARN, "hci", "malformed LE LTK request event");
    return CommandChannel::EventCallbackResult::kContinue;
  }

  hci_spec::ConnectionHandle handle = le16toh(params->connection_handle);

  // Silently ignore the event as it isn't meant for this connection.
  if (handle != this->handle()) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  std::unique_ptr<CommandPacket> cmd;

  uint64_t rand = le64toh(params->random_number);
  uint16_t ediv = le16toh(params->encrypted_diversifier);

  bt_log(DEBUG, "hci", "LE LTK request - ediv: %#.4x, rand: %#.16lx", ediv, rand);
  if (ltk() && ltk()->rand() == rand && ltk()->ediv() == ediv) {
    cmd = CommandPacket::New(hci_spec::kLELongTermKeyRequestReply,
                             sizeof(hci_spec::LELongTermKeyRequestReplyCommandParams));
    auto* params = cmd->mutable_payload<hci_spec::LELongTermKeyRequestReplyCommandParams>();

    params->connection_handle = htole16(handle);
    params->long_term_key = ltk()->value();
  } else {
    bt_log(DEBUG, "hci-le", "LTK request rejected");

    cmd = CommandPacket::New(hci_spec::kLELongTermKeyRequestNegativeReply,
                             sizeof(hci_spec::LELongTermKeyRequestNegativeReplyCommandParams));
    auto* params = cmd->mutable_payload<hci_spec::LELongTermKeyRequestNegativeReplyCommandParams>();
    params->connection_handle = htole16(handle);
  }

  auto status_cb = [](auto id, const EventPacket& event) {
    hci_is_error(event, TRACE, "hci-le", "failed to reply to LTK request");
  };
  hci()->command_channel()->SendCommand(std::move(cmd), std::move(status_cb));
  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
