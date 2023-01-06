// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTION_H_

#include "acl_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/le_connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/link_key.h"

namespace bt::hci {

class LowEnergyConnection : public AclConnection, public WeakSelf<LowEnergyConnection> {
 public:
  LowEnergyConnection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                      const DeviceAddress& peer_address,
                      const hci_spec::LEConnectionParameters& params, hci_spec::ConnectionRole role,
                      const Transport::WeakPtr& hci);

  ~LowEnergyConnection() override;

  // Authenticate (i.e. encrypt) this connection using its current link key.  Returns false if the
  // procedure cannot be initiated. The result of the authentication procedure will be reported via
  // the encryption change callback.  If the link layer procedure fails, the connection will be
  // disconnected. The encryption change callback will be notified of the failure.
  bool StartEncryption() override;

  // Sets the active LE parameters of this connection.
  void set_low_energy_parameters(const hci_spec::LEConnectionParameters& params) {
    parameters_ = params;
  }

  // The active LE connection parameters of this connection.
  const hci_spec::LEConnectionParameters& low_energy_parameters() const { return parameters_; }

  using AclConnection::set_ltk;

 private:
  void HandleEncryptionStatus(Result<bool /*enabled*/> result, bool key_refreshed) override;

  // HCI event handlers.
  CommandChannel::EventCallbackResult OnLELongTermKeyRequestEvent(const EventPacket& event);

  // IDs for encryption related HCI event handlers.
  CommandChannel::EventHandlerId le_ltk_request_id_;

  hci_spec::LEConnectionParameters parameters_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTION_H_
