// Copyright 2020 The Fuchsia Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_HANDLE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_HANDLE_H_

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::gap {

namespace internal {
class LowEnergyConnection;
}

class LowEnergyConnectionManager;

class LowEnergyConnectionHandle final {
 public:
  // |release_cb| will be called when this handle releases its reference to the connection.
  // |bondable_cb| returns the current bondable mode of the connection. It will only be called while
  // the connection is active.
  // |security_mode| returns the current security properties of the connection. It will only be
  // called while the connection is active.
  LowEnergyConnectionHandle(PeerId peer_id, hci_spec::ConnectionHandle handle,
                            fit::callback<void(LowEnergyConnectionHandle*)> release_cb,
                            fit::function<sm::BondableMode()> bondable_cb,
                            fit::function<sm::SecurityProperties()> security_cb);

  // Destroying this object releases its reference to the underlying connection.
  ~LowEnergyConnectionHandle();

  // Releases this object's reference to the underlying connection.
  void Release();

  // Returns true if the underlying connection is still active.
  bool active() const { return active_; }

  // Sets a callback to be called when the underlying connection is closed.
  void set_closed_callback(fit::closure callback) { closed_cb_ = std::move(callback); }

  // Returns the operational bondable mode of the underlying connection. See spec V5.1 Vol 3 Part
  // C Section 9.4 for more details.
  sm::BondableMode bondable_mode() const;

  sm::SecurityProperties security() const;

  PeerId peer_identifier() const { return peer_id_; }
  hci_spec::ConnectionHandle handle() const { return handle_; }

 private:
  friend class LowEnergyConnectionManager;
  friend class internal::LowEnergyConnection;

  // Called by LowEnergyConnectionManager when the underlying connection is
  // closed. Notifies |closed_cb_|.
  void MarkClosed();

  bool active_;
  PeerId peer_id_;
  hci_spec::ConnectionHandle handle_;
  fit::closure closed_cb_;
  fit::callback<void(LowEnergyConnectionHandle*)> release_cb_;
  fit::function<sm::BondableMode()> bondable_cb_;
  fit::function<sm::SecurityProperties()> security_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionHandle);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_HANDLE_H_
