// Copyright 2020 The Fuchsia Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_handle.h"

#include "low_energy_connection.h"
#include "low_energy_connection_manager.h"

namespace bt::gap {

LowEnergyConnectionHandle::LowEnergyConnectionHandle(
    PeerId peer_id, hci_spec::ConnectionHandle handle,
    fxl::WeakPtr<LowEnergyConnectionManager> manager)
    : active_(true), peer_id_(peer_id), handle_(handle), manager_(manager) {
  ZX_DEBUG_ASSERT(peer_id_.IsValid());
  ZX_DEBUG_ASSERT(manager_);
  ZX_DEBUG_ASSERT(handle_);
}

LowEnergyConnectionHandle::~LowEnergyConnectionHandle() {
  ZX_ASSERT(thread_checker_.is_thread_valid());
  if (active_) {
    Release();
  }
}

void LowEnergyConnectionHandle::Release() {
  ZX_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(active_);
  active_ = false;
  if (manager_) {
    manager_->ReleaseReference(this);
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
  ZX_DEBUG_ASSERT(manager_);
  auto conn_iter = manager_->connections_.find(peer_id_);
  ZX_DEBUG_ASSERT(conn_iter != manager_->connections_.end());
  return conn_iter->second->bondable_mode();
}

sm::SecurityProperties LowEnergyConnectionHandle::security() const {
  ZX_DEBUG_ASSERT(manager_);
  auto conn_iter = manager_->connections_.find(peer_id_);
  ZX_DEBUG_ASSERT(conn_iter != manager_->connections_.end());
  return conn_iter->second->security();
}

}  // namespace bt::gap
