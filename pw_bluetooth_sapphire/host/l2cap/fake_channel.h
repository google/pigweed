// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_H_

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::l2cap::testing {

// FakeChannel is a simple pass-through Channel implementation that is intended
// for L2CAP service level unit tests where data is transmitted over a L2CAP
// channel.
class FakeChannel : public Channel {
 public:
  FakeChannel(ChannelId id, ChannelId remote_id, hci_spec::ConnectionHandle handle,
              bt::LinkType link_type,
              ChannelInfo info = ChannelInfo::MakeBasicMode(kDefaultMTU, kDefaultMTU));
  ~FakeChannel() override = default;

  // Routes the given data over to the rx handler as if it were received from
  // the controller.
  void Receive(const ByteBuffer& data);

  // Sets a delegate to notify when a frame was sent over the channel.
  using SendCallback = fit::function<void(ByteBufferPtr)>;
  void SetSendCallback(SendCallback callback, async_dispatcher_t* dispatcher);

  // Sets a callback to emulate the result of "SignalLinkError()". In
  // production, this callback is invoked by the link.
  void SetLinkErrorCallback(LinkErrorCallback callback);

  // Sets a callback to emulate the result of "UpgradeSecurity()".
  void SetSecurityCallback(SecurityUpgradeCallback callback, async_dispatcher_t* dispatcher);

  // Emulates channel closure.
  void Close();

  fxl::WeakPtr<FakeChannel> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // Activating always fails if true.
  void set_activate_fails(bool value) { activate_fails_ = value; }

  // True if SignalLinkError() has been called.
  bool link_error() const { return link_error_; }

  // True if Deactivate has yet not been called after Activate.
  bool activated() const { return static_cast<bool>(rx_cb_); }

  // Assigns a link security level.
  void set_security(const sm::SecurityProperties& sec_props) { security_ = sec_props; }

  // RequestAclPriority always fails if true.
  void set_acl_priority_fails(bool fail) { acl_priority_fails_ = fail; }

  void set_flush_timeout_succeeds(bool succeed) { flush_timeout_succeeds_ = succeed; }

  // Channel overrides:
  const sm::SecurityProperties security() override { return security_; }
  bool Activate(RxCallback rx_callback, ClosedCallback closed_callback) override;
  void Deactivate() override;
  void SignalLinkError() override;
  bool Send(ByteBufferPtr sdu) override;
  void UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) override;
  void RequestAclPriority(hci::AclPriority priority,
                          fit::callback<void(fitx::result<fitx::failed>)> cb) override;
  void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                     hci::ResultCallback<> callback) override;
  void AttachInspect(inspect::Node& parent, std::string name) override {}
  fxl::WeakPtr<Channel> GetWeakPtr() override { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  hci_spec::ConnectionHandle handle_;
  Fragmenter fragmenter_;

  sm::SecurityProperties security_;
  SecurityUpgradeCallback security_cb_;
  async_dispatcher_t* security_dispatcher_;

  ClosedCallback closed_cb_;
  RxCallback rx_cb_;

  SendCallback send_cb_;
  async_dispatcher_t* send_dispatcher_;

  LinkErrorCallback link_err_cb_;

  bool activate_fails_;
  bool link_error_;

  bool acl_priority_fails_;
  bool flush_timeout_succeeds_ = true;

  // The pending SDUs on this channel. Received PDUs are buffered if |rx_cb_| is
  // currently not set.
  std::queue<ByteBufferPtr> pending_rx_sdus_;

  fxl::WeakPtrFactory<FakeChannel> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeChannel);
};

}  // namespace bt::l2cap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_H_
