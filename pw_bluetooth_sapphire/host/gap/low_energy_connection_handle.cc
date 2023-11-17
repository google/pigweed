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

#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_connection_handle.h"

#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_connection.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_connection_manager.h"

namespace bt::gap {

LowEnergyConnectionHandle::LowEnergyConnectionHandle(
    PeerId peer_id,
    hci_spec::ConnectionHandle handle,
    fit::callback<void(LowEnergyConnectionHandle*)> release_cb,
    fit::function<sm::BondableMode()> bondable_cb,
    fit::function<sm::SecurityProperties()> security_cb)
    : active_(true),
      peer_id_(peer_id),
      handle_(handle),
      release_cb_(std::move(release_cb)),
      bondable_cb_(std::move(bondable_cb)),
      security_cb_(std::move(security_cb)) {
  BT_ASSERT(peer_id_.IsValid());
}

LowEnergyConnectionHandle::~LowEnergyConnectionHandle() {
  if (active_) {
    Release();
  }
}

void LowEnergyConnectionHandle::Release() {
  BT_ASSERT(active_);
  active_ = false;
  if (release_cb_) {
    release_cb_(this);
  }
}

void LowEnergyConnectionHandle::MarkClosed() {
  active_ = false;
  if (closed_cb_) {
    // Move the callback out of |closed_cb_| to prevent it from deleting itself
    // by deleting |this|.
    auto f = std::move(closed_cb_);
    f();
  }
}

sm::BondableMode LowEnergyConnectionHandle::bondable_mode() const {
  BT_ASSERT(active_);
  return bondable_cb_();
}

sm::SecurityProperties LowEnergyConnectionHandle::security() const {
  BT_ASSERT(active_);
  return security_cb_();
}

}  // namespace bt::gap
