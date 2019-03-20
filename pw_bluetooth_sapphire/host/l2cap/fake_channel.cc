// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_channel.h"

#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace l2cap {
namespace testing {

FakeChannel::FakeChannel(ChannelId id, ChannelId remote_id,
                         hci::ConnectionHandle handle,
                         hci::Connection::LinkType link_type)
    : Channel(id, remote_id, link_type, handle),
      handle_(handle),
      fragmenter_(handle),
      dispatcher_(nullptr),
      send_dispatcher_(nullptr),
      link_err_dispatcher_(nullptr),
      activate_fails_(false),
      link_error_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(handle_);
}

void FakeChannel::Receive(const common::ByteBuffer& data) {
  ZX_DEBUG_ASSERT(!!rx_cb_ == !!dispatcher_);

  auto pdu = fragmenter_.BuildBasicFrame(id(), data);
  auto sdu = std::make_unique<common::DynamicByteBuffer>(pdu.length());
  pdu.Copy(sdu.get());
  if (dispatcher_) {
    async::PostTask(dispatcher_,
                    [cb = rx_cb_.share(), sdu = std::move(sdu)]() mutable {
                      cb(std::move(sdu));
                    });
  } else {
    pending_rx_sdus_.push(std::move(sdu));
  }
}

void FakeChannel::SetSendCallback(SendCallback callback,
                                  async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(static_cast<bool>(callback) == static_cast<bool>(dispatcher));

  send_cb_ = std::move(callback);
  send_dispatcher_ = dispatcher;
}

void FakeChannel::SetLinkErrorCallback(LinkErrorCallback callback,
                                       async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(static_cast<bool>(callback) == static_cast<bool>(dispatcher));

  link_err_cb_ = std::move(callback);
  link_err_dispatcher_ = dispatcher;
}

void FakeChannel::SetSecurityCallback(SecurityUpgradeCallback callback,
                                      async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(static_cast<bool>(callback) == static_cast<bool>(dispatcher));

  security_cb_ = std::move(callback);
  security_dispatcher_ = dispatcher;
}

void FakeChannel::Close() {
  if (closed_cb_)
    closed_cb_();
}

bool FakeChannel::Activate(RxCallback rx_callback,
                           ClosedCallback closed_callback,
                           async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(rx_callback);
  ZX_DEBUG_ASSERT(closed_callback);
  ZX_DEBUG_ASSERT(!rx_cb_);
  ZX_DEBUG_ASSERT(!closed_cb_);

  if (activate_fails_)
    return false;

  dispatcher_ = dispatcher;
  closed_cb_ = std::move(closed_callback);
  rx_cb_ = std::move(rx_callback);

  while (!pending_rx_sdus_.empty()) {
    rx_cb_(std::move(pending_rx_sdus_.front()));
    pending_rx_sdus_.pop();
  }

  return true;
}

void FakeChannel::Deactivate() {
  dispatcher_ = nullptr;
  closed_cb_ = {};
  rx_cb_ = {};
}

void FakeChannel::SignalLinkError() {
  link_error_ = true;

  if (link_err_cb_) {
    async::PostTask(link_err_dispatcher_, link_err_cb_.share());
  }
}

bool FakeChannel::Send(common::ByteBufferPtr sdu) {
  ZX_DEBUG_ASSERT(sdu);

  if (!send_cb_)
    return false;

  if (sdu->size() > tx_mtu()) {
    bt_log(ERROR, "l2cap",
           "Dropping oversized SDU (sdu->size()=%u, tx_mtu()=%u)", sdu->size(),
           tx_mtu());
    return false;
  }

  ZX_DEBUG_ASSERT(send_dispatcher_);
  async::PostTask(send_dispatcher_,
                  [cb = send_cb_.share(), sdu = std::move(sdu)]() mutable {
                    cb(std::move(sdu));
                  });

  return true;
}

void FakeChannel::UpgradeSecurity(sm::SecurityLevel level,
                                  sm::StatusCallback callback) {
  ZX_DEBUG_ASSERT(security_dispatcher_);
  async::PostTask(
      security_dispatcher_,
      [cb = std::move(callback), f = security_cb_.share(), handle = handle_,
       level]() mutable { f(handle, level, std::move(cb)); });
}

}  // namespace testing
}  // namespace l2cap
}  // namespace bt
