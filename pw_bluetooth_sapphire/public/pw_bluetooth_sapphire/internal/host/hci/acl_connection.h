// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_CONNECTION_H_

#include "pw_bluetooth_sapphire/internal/host/hci-spec/link_key.h"
#include "pw_bluetooth_sapphire/internal/host/hci/connection.h"

namespace bt::hci {

// Represents an ACL-U or LE-U link, both of which use the ACL data channel and
// support encryption procedures.
// Concrete implementations are found in BrEdrConnection and
// LowEnergyConnection.
class AclConnection : public Connection {
 public:
  ~AclConnection() override;

  // Authenticate (i.e. encrypt) this connection using its current link key.
  // Returns false if the procedure cannot be initiated. The result of the
  // authentication procedure will be reported via the encryption change
  // callback.
  //
  // If the link layer procedure fails, the connection will be disconnected. The
  // encryption change callback will be notified of the failure.
  virtual bool StartEncryption() = 0;

  // Assigns a callback that will run when the encryption state of the
  // underlying link changes. The bool value parameter represents the new state.
  void set_encryption_change_callback(ResultFunction<bool> callback) {
    encryption_change_callback_ = std::move(callback);
  }

  // Returns the role of the local device in the established connection.
  pw::bluetooth::emboss::ConnectionRole role() const { return role_; }

  // Update the role of the local device when a role change occurs.
  void set_role(pw::bluetooth::emboss::ConnectionRole role) { role_ = role; }

  // The current long term key of the connection.
  const std::optional<hci_spec::LinkKey>& ltk() const { return ltk_; }

  void set_use_secure_connections(bool use_secure_connections) {
    use_secure_connections_ = use_secure_connections;
  }

  pw::bluetooth::emboss::EncryptionStatus encryption_status() const {
    return encryption_status_;
  }

 protected:
  AclConnection(hci_spec::ConnectionHandle handle,
                const DeviceAddress& local_address,
                const DeviceAddress& peer_address,
                pw::bluetooth::emboss::ConnectionRole role,
                const Transport::WeakPtr& hci);

  void set_ltk(const hci_spec::LinkKey& link_key) { ltk_ = link_key; }

  // Notifies subclasses of a change in encryption status.
  virtual void HandleEncryptionStatus(Result<bool /*enabled*/> result,
                                      bool key_refreshed) = 0;

  ResultFunction<bool>& encryption_change_callback() {
    return encryption_change_callback_;
  }

 private:
  // This method must be static since it may be invoked after the connection
  // associated with it is destroyed.
  static void OnDisconnectionComplete(hci_spec::ConnectionHandle handle,
                                      const Transport::WeakPtr& hci);

  // HCI event handlers.
  CommandChannel::EventCallbackResult OnEncryptionChangeEvent(
      const EmbossEventPacket& event);
  CommandChannel::EventCallbackResult OnEncryptionKeyRefreshCompleteEvent(
      const EmbossEventPacket& event);

  // IDs for encryption related HCI event handlers.
  CommandChannel::EventHandlerId enc_change_id_;
  CommandChannel::EventHandlerId enc_key_refresh_cmpl_id_;

  // This connection's current link key.
  std::optional<hci_spec::LinkKey> ltk_;

  // Flag indicating if peer and local Secure Connections support are both
  // present. Set in OnLinkKeyNotification in PairingState
  bool use_secure_connections_ = false;

  pw::bluetooth::emboss::EncryptionStatus encryption_status_ =
      pw::bluetooth::emboss::EncryptionStatus::OFF;

  pw::bluetooth::emboss::ConnectionRole role_;

  ResultFunction<bool> encryption_change_callback_;

  WeakSelf<AclConnection> weak_self_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_CONNECTION_H_
