// Copyright 2023 The Pigweed Authors
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
#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::gap {

namespace internal {
class LowEnergyConnection;
}

class LowEnergyConnectionManager;

class LowEnergyConnectionHandle final {
 public:
  // |release_cb| will be called when this handle releases its reference to the
  // connection. |bondable_cb| returns the current bondable mode of the
  // connection. It will only be called while the connection is active.
  // |security_mode| returns the current security properties of the connection.
  // It will only be called while the connection is active.
  LowEnergyConnectionHandle(
      PeerId peer_id,
      hci_spec::ConnectionHandle handle,
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
  void set_closed_callback(fit::closure callback) {
    closed_cb_ = std::move(callback);
  }

  // Returns the operational bondable mode of the underlying connection. See
  // spec V5.1 Vol 3 Part C Section 9.4 for more details.
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

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionHandle);
};

}  // namespace bt::gap
