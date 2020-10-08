// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection.h"

namespace bt::gap {

ScoConnection::ScoConnection(std::unique_ptr<hci::Connection> connection,
                             fit::closure deactivated_cb)
    : active_(false),
      connection_(std::move(connection)),
      deactivated_cb_(std::move(deactivated_cb)) {
  ZX_ASSERT(connection_);
  ZX_ASSERT(connection_->ll_type() == hci::Connection::LinkType::kSCO ||
            connection_->ll_type() == hci::Connection::LinkType::kESCO);
  handle_ = connection_->handle();
}

fbl::RefPtr<ScoConnection> ScoConnection::Create(std::unique_ptr<hci::Connection> connection,
                                                 fit::closure closed_cb) {
  return fbl::AdoptRef(new ScoConnection(std::move(connection), std::move(closed_cb)));
}

ScoConnection::UniqueId ScoConnection::unique_id() const {
  // HCI connection handles are unique per controller.
  return handle();
}

ScoConnection::UniqueId ScoConnection::id() const { return unique_id(); }

void ScoConnection::Close() {
  bt_log(TRACE, "gap-sco", "closing sco connection (handle: %.4x)", handle());

  bool active = active_;
  CleanUp();

  if (!active) {
    return;
  }

  ZX_ASSERT(activator_closed_cb_);
  activator_closed_cb_();
  activator_closed_cb_ = nullptr;
}

bool ScoConnection::Activate(ScoConnection::RxCallback /*rx_callback*/,
                             fit::closure closed_callback) {
  // TODO(fxbug.dev/58458): Handle Activate() called on a connection that has been closed already.
  ZX_ASSERT(closed_callback);
  ZX_ASSERT(!active_);
  activator_closed_cb_ = std::move(closed_callback);
  active_ = true;
  return true;
}

void ScoConnection::Deactivate() {
  bt_log(TRACE, "gap-sco", "deactivating sco connection (handle: %.4x)", handle());
  CleanUp();
  if (deactivated_cb_) {
    deactivated_cb_();
    deactivated_cb_ = nullptr;
  }
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
