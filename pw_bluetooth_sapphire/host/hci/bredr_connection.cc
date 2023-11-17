// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/bredr_connection.h"

#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

BrEdrConnection::BrEdrConnection(hci_spec::ConnectionHandle handle,
                                 const DeviceAddress& local_address,
                                 const DeviceAddress& peer_address,
                                 pw::bluetooth::emboss::ConnectionRole role,
                                 const Transport::WeakPtr& hci)
    : AclConnection(handle, local_address, peer_address, role, hci),
      WeakSelf(this) {
  BT_ASSERT(local_address.type() == DeviceAddress::Type::kBREDR);
  BT_ASSERT(peer_address.type() == DeviceAddress::Type::kBREDR);
  BT_ASSERT(hci.is_alive());
  BT_ASSERT(hci->acl_data_channel());
}

bool BrEdrConnection::StartEncryption() {
  if (state() != Connection::State::kConnected) {
    bt_log(DEBUG, "hci", "connection closed; cannot start encryption");
    return false;
  }

  BT_ASSERT(ltk().has_value() == ltk_type_.has_value());
  if (!ltk().has_value()) {
    bt_log(
        DEBUG,
        "hci",
        "connection link key type has not been set; not starting encryption");
    return false;
  }

  auto cmd = EmbossCommandPacket::New<
      pw::bluetooth::emboss::SetConnectionEncryptionCommandWriter>(
      hci_spec::kSetConnectionEncryption);
  auto params = cmd.view_t();
  params.connection_handle().Write(handle());
  params.encryption_enable().Write(
      pw::bluetooth::emboss::GenericEnableParam::ENABLE);

  auto self = GetWeakPtr();
  auto event_cb = [self, handle = handle()](auto id, const EventPacket& event) {
    if (!self.is_alive()) {
      return;
    }

    Result<> result = event.ToResult();
    if (bt_is_error(result,
                    ERROR,
                    "hci-bredr",
                    "could not set encryption on link %#.04x",
                    handle)) {
      if (self->encryption_change_callback()) {
        self->encryption_change_callback()(result.take_error());
      }
      return;
    }
    bt_log(DEBUG, "hci-bredr", "requested encryption start on %#.04x", handle);
  };

  if (!hci().is_alive()) {
    return false;
  }
  return hci()->command_channel()->SendCommand(
      std::move(cmd), std::move(event_cb), hci_spec::kCommandStatusEventCode);
}

void BrEdrConnection::HandleEncryptionStatus(Result<bool> result,
                                             bool key_refreshed) {
  bool enabled = result.is_ok() && result.value() && !key_refreshed;
  if (enabled) {
    ValidateEncryptionKeySize([self = GetWeakPtr()](Result<> key_valid_status) {
      if (self.is_alive()) {
        self->HandleEncryptionStatusValidated(
            key_valid_status.is_ok() ? Result<bool>(fit::ok(true))
                                     : key_valid_status.take_error());
      }
    });
    return;
  }
  HandleEncryptionStatusValidated(result);
}

void BrEdrConnection::HandleEncryptionStatusValidated(Result<bool> result) {
  // Core Spec Vol 3, Part C, 5.2.2.1.1 and 5.2.2.2.1 mention disconnecting the
  // link after pairing failures (supported by TS GAP/SEC/SEM/BV-10-C), but do
  // not specify actions to take after encryption failures. We'll choose to
  // disconnect ACL links after encryption failure.
  if (result.is_error()) {
    Disconnect(pw::bluetooth::emboss::StatusCode::AUTHENTICATION_FAILURE);
  }

  if (!encryption_change_callback()) {
    bt_log(DEBUG,
           "hci",
           "%#.4x: no encryption status callback assigned",
           handle());
    return;
  }
  encryption_change_callback()(result);
}

void BrEdrConnection::ValidateEncryptionKeySize(
    hci::ResultFunction<> key_size_validity_cb) {
  BT_ASSERT(state() == Connection::State::kConnected);

  auto cmd = EmbossCommandPacket::New<
      pw::bluetooth::emboss::ReadEncryptionKeySizeCommandWriter>(
      hci_spec::kReadEncryptionKeySize);
  cmd.view_t().connection_handle().Write(handle());

  auto event_cb = [self = GetWeakPtr(),
                   valid_cb = std::move(key_size_validity_cb)](
                      auto, const EventPacket& event) {
    if (!self.is_alive()) {
      return;
    }

    Result<> result = event.ToResult();
    if (!bt_is_error(result,
                     ERROR,
                     "hci",
                     "Could not read ACL encryption key size on %#.4x",
                     self->handle())) {
      const auto& return_params =
          *event.return_params<hci_spec::ReadEncryptionKeySizeReturnParams>();
      const auto key_size = return_params.key_size;
      bt_log(TRACE,
             "hci",
             "%#.4x: encryption key size %hhu",
             self->handle(),
             key_size);

      if (key_size < hci_spec::kMinEncryptionKeySize) {
        bt_log(WARN,
               "hci",
               "%#.4x: encryption key size %hhu insufficient",
               self->handle(),
               key_size);
        result = ToResult(HostError::kInsufficientSecurity);
      }
    }
    valid_cb(result);
  };
  hci()->command_channel()->SendCommand(std::move(cmd), std::move(event_cb));
}

}  // namespace bt::hci
