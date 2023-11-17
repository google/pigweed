// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_H_

#include "pw_bluetooth_sapphire/internal/host/hci-spec/link_key.h"
#include "pw_bluetooth_sapphire/internal/host/hci/acl_connection.h"

namespace bt::hci {

// BrEdrConnection represents a BR/EDR logical link connection to a peer. In
// addition to general link lifetime and encryption procedures provided by
// AclConnection, BrEdrConnection manages BR/EDR-specific encryption procedures.
class BrEdrConnection : public AclConnection, public WeakSelf<BrEdrConnection> {
 public:
  BrEdrConnection(hci_spec::ConnectionHandle handle,
                  const DeviceAddress& local_address,
                  const DeviceAddress& peer_address,
                  pw::bluetooth::emboss::ConnectionRole role,
                  const Transport::WeakPtr& hci);

  bool StartEncryption() override;

  // Assigns a link key with its corresponding HCI type to this BR/EDR
  // connection. This will be used for bonding procedures and determines the
  // resulting security properties of the link.
  void set_link_key(const hci_spec::LinkKey& link_key,
                    hci_spec::LinkKeyType type) {
    set_ltk(link_key);
    ltk_type_ = type;
  }

  const std::optional<hci_spec::LinkKeyType>& ltk_type() { return ltk_type_; }

 private:
  void HandleEncryptionStatus(Result<bool /*enabled*/> result,
                              bool key_refreshed) override;

  void HandleEncryptionStatusValidated(Result<bool> result);

  void ValidateEncryptionKeySize(hci::ResultFunction<> key_size_validity_cb);

  // BR/EDR-specific type of the assigned link key.
  std::optional<hci_spec::LinkKeyType> ltk_type_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_BREDR_CONNECTION_H_
