// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_channel.h"

#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::l2cap::testing {

FakeChannel::FakeChannel(ChannelId id, ChannelId remote_id, hci_spec::ConnectionHandle handle,
                         bt::LinkType link_type, ChannelInfo info)
    : Channel(id, remote_id, link_type, handle, info),
      handle_(handle),
      fragmenter_(handle),
      send_dispatcher_(nullptr),
      activate_fails_(false),
      link_error_(false),
      acl_priority_fails_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(handle_);
}

void FakeChannel::Receive(const ByteBuffer& data) {
  auto pdu = fragmenter_.BuildFrame(id(), data, FrameCheckSequenceOption::kNoFcs);
  auto sdu = std::make_unique<DynamicByteBuffer>(pdu.length());
  pdu.Copy(sdu.get());
  if (rx_cb_) {
    rx_cb_(std::move(sdu));
  } else {
    pending_rx_sdus_.push(std::move(sdu));
  }
}

void FakeChannel::SetSendCallback(SendCallback callback, async_dispatcher_t* dispatcher) {
  send_cb_ = std::move(callback);
  send_dispatcher_ = dispatcher;
}

void FakeChannel::SetLinkErrorCallback(LinkErrorCallback callback) {
  link_err_cb_ = std::move(callback);
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

bool FakeChannel::Activate(RxCallback rx_callback, ClosedCallback closed_callback) {
  ZX_DEBUG_ASSERT(rx_callback);
  ZX_DEBUG_ASSERT(closed_callback);
  ZX_DEBUG_ASSERT(!rx_cb_);
  ZX_DEBUG_ASSERT(!closed_cb_);

  if (activate_fails_)
    return false;

  closed_cb_ = std::move(closed_callback);
  rx_cb_ = std::move(rx_callback);

  while (!pending_rx_sdus_.empty()) {
    rx_cb_(std::move(pending_rx_sdus_.front()));
    pending_rx_sdus_.pop();
  }

  return true;
}

void FakeChannel::Deactivate() {
  closed_cb_ = {};
  rx_cb_ = {};
}

void FakeChannel::SignalLinkError() {
  link_error_ = true;

  if (link_err_cb_) {
    link_err_cb_();
  }
}

bool FakeChannel::Send(ByteBufferPtr sdu) {
  ZX_DEBUG_ASSERT(sdu);

  if (!send_cb_)
    return false;

  if (sdu->size() > max_tx_sdu_size()) {
    bt_log(ERROR, "l2cap", "Dropping oversized SDU (sdu->size()=%zu, max_tx_sdu_size()=%u)",
           sdu->size(), max_tx_sdu_size());
    return false;
  }

  if (send_dispatcher_) {
    async::PostTask(send_dispatcher_, [cb = send_cb_.share(), sdu = std::move(sdu)]() mutable {
      cb(std::move(sdu));
    });
  } else {
    send_cb_(std::move(sdu));
  }

  return true;
}

void FakeChannel::UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) {
  ZX_ASSERT(security_dispatcher_);
  async::PostTask(security_dispatcher_,
                  [cb = std::move(callback), f = security_cb_.share(), handle = handle_,
                   level]() mutable { f(handle, level, std::move(cb)); });
}

void FakeChannel::RequestAclPriority(hci::AclPriority priority,
                                     fit::callback<void(fitx::result<fitx::failed>)> cb) {
  if (acl_priority_fails_) {
    cb(fitx::failed());
    return;
  }
  requested_acl_priority_ = priority;
  cb(fitx::ok());
}

void FakeChannel::SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                                hci::ResultCallback<> callback) {
  if (!flush_timeout_succeeds_) {
    callback(ToResult(hci_spec::StatusCode::kUnspecifiedError));
    return;
  }
  info_.flush_timeout = flush_timeout;
  callback(fitx::ok());
}

}  // namespace bt::l2cap::testing
