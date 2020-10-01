// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection.h"

namespace bt::gap {

ScoConnection::ScoConnection(std::unique_ptr<hci::Connection> connection)
    : active_(false), connection_(std::move(connection)) {
  ZX_ASSERT(connection_);
  ZX_ASSERT(connection_->ll_type() == hci::Connection::LinkType::kSCO ||
            connection_->ll_type() == hci::Connection::LinkType::kESCO);
}

fbl::RefPtr<ScoConnection> ScoConnection::Create(std::unique_ptr<hci::Connection> connection) {
  return fbl::AdoptRef(new ScoConnection(std::move(connection)));
}

ScoConnection::UniqueId ScoConnection::unique_id() const {
  // HCI connection handles are unique per controller.
  return handle();
}

ScoConnection::UniqueId ScoConnection::id() const { return unique_id(); }

void ScoConnection::Close() {
  if (!active_) {
    return;
  }

  bt_log(TRACE, "gap-sco", "closing sco connection (handle: %.4x)", handle());

  ZX_ASSERT(closed_cb_);
  auto closed_cb = std::move(closed_cb_);

  CleanUp();

  closed_cb();
}

bool ScoConnection::Activate(ScoConnection::RxCallback /*rx_callback*/,
                             fit::closure closed_callback) {
  ZX_ASSERT(closed_callback);
  ZX_ASSERT(!active_);
  closed_cb_ = std::move(closed_callback);
  active_ = true;
  return true;
}

void ScoConnection::Deactivate() {
  if (!active_) {
    return;
  }

  bt_log(TRACE, "gap-sco", "deactivating sco connection (handle: %.4x)", handle());

  CleanUp();
}

uint16_t ScoConnection::max_tx_sdu_size() const {
  bt_log(WARN, "gap-sco", "%s is not supported", __func__);
  return 0u;
}

bool ScoConnection::Send(ByteBufferPtr sdu) {
  bt_log(WARN, "gap-sco", "%s is not supported", __func__);
  return false;
}

void ScoConnection::CleanUp() {
  active_ = false;
  connection_ = nullptr;
}

}  // namespace bt::gap
