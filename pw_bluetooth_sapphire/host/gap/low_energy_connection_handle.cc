// Copyright 2020 The Fuchsia Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_handle.h"

#include "low_energy_connection.h"
#include "low_energy_connection_manager.h"

namespace bt::gap {

LowEnergyConnectionHandle::LowEnergyConnectionHandle(
    PeerId peer_id, hci_spec::ConnectionHandle handle,
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
  BT_ASSERT(handle_);
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
