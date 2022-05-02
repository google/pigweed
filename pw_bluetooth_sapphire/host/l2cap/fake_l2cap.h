// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_L2CAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_L2CAP_H_

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"

namespace bt::l2cap::testing {

class FakeChannel;

// This is a fake version of the ChannelManager class that can be injected into other
// layers for unit testing.
class FakeL2cap final : public ChannelManager {
 public:
  FakeL2cap() = default;
  ~FakeL2cap() override;

  void AttachInspect(inspect::Node& parent, std::string name) override {}

  // Returns true if and only if a link identified by |handle| has been added and connected.
  [[nodiscard]] bool IsLinkConnected(hci_spec::ConnectionHandle handle) const;

  // Triggers a LE connection parameter update callback on the given link.
  void TriggerLEConnectionParameterUpdate(hci_spec::ConnectionHandle handle,
                                          const hci_spec::LEPreferredConnectionParameters& params);

  // Sets up the expectation that an outbound dynamic channel will be opened
  // on the given link. Each call will expect one dyanmic channel to be
  // created.  If a call to OpenL2capChannel is made without expectation, it
  // will assert.
  // Multiple expectations for the same PSM should be queued in FIFO order.
  void ExpectOutboundL2capChannel(hci_spec::ConnectionHandle handle, PSM psm, ChannelId id,
                                  ChannelId remote_id, ChannelParameters params);

  // Triggers the creation of an inbound dynamic channel on the given link. The
  // channels created will be provided to handlers passed to RegisterService.
  // Returns false if unable to create the channel.
  bool TriggerInboundL2capChannel(hci_spec::ConnectionHandle handle, PSM psm, ChannelId id,
                                  ChannelId remote_id, uint16_t tx_mtu = kDefaultMTU);

  // Triggers a link error callback on the given link.
  void TriggerLinkError(hci_spec::ConnectionHandle handle);

  // L2cap overrides:
  void AddACLConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                        LinkErrorCallback link_error_callback,
                        SecurityUpgradeCallback security_callback) override;
  LEFixedChannels AddLEConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                                  LinkErrorCallback link_error_callback,
                                  LEConnectionParameterUpdateCallback conn_param_callback,
                                  SecurityUpgradeCallback security_callback) override;
  void RemoveConnection(hci_spec::ConnectionHandle handle) override;
  void AssignLinkSecurityProperties(hci_spec::ConnectionHandle handle,
                                    sm::SecurityProperties security) override;

  // Immediately posts accept on |dispatcher|.
  void RequestConnectionParameterUpdate(
      hci_spec::ConnectionHandle handle, hci_spec::LEPreferredConnectionParameters params,
      ConnectionParameterUpdateRequestCallback request_cb) override;

  fbl::RefPtr<Channel> OpenFixedChannel(hci_spec::ConnectionHandle connection_handle,
                                        ChannelId channel_id) override {
    return nullptr;
  }
  void OpenL2capChannel(hci_spec::ConnectionHandle handle, PSM psm, ChannelParameters params,
                        ChannelCallback cb) override;
  bool RegisterService(PSM psm, ChannelParameters params,
                       ChannelCallback channel_callback) override;
  void UnregisterService(PSM psm) override;

  fxl::WeakPtr<internal::LogicalLink> LogicalLinkForTesting(
      hci_spec::ConnectionHandle handle) override {
    return nullptr;
  }

  // Called when a new channel gets opened. Tests can use this to obtain a
  // reference to all channels.
  using FakeChannelCallback = fit::function<void(fbl::RefPtr<testing::FakeChannel>)>;
  void set_channel_callback(FakeChannelCallback callback) { chan_cb_ = std::move(callback); }
  void set_simulate_open_channel_failure(bool simulate_failure) {
    simulate_open_channel_failure_ = simulate_failure;
  }

  // Called when RequestConnectionParameterUpdate is called. |request_cb| will be called with return
  // value. Defaults to returning true if not set.
  using ConnectionParameterUpdateRequestResponder =
      fit::function<bool(hci_spec::ConnectionHandle, hci_spec::LEPreferredConnectionParameters)>;
  void set_connection_parameter_update_request_responder(
      ConnectionParameterUpdateRequestResponder responder) {
    connection_parameter_update_request_responder_ = std::move(responder);
  }

 private:
  // TODO(armansito): Consider moving the following logic into an internal fake
  // that is L2CAP-specific.
  struct ChannelData {
    ChannelId local_id;
    ChannelId remote_id;
    ChannelParameters params;
  };
  struct LinkData {
    // Expectations on links can be created before they are connected.
    bool connected;
    hci_spec::ConnectionHandle handle;
    hci_spec::ConnectionRole role;
    bt::LinkType type;

    async_dispatcher_t* dispatcher;

    // Dual-mode callbacks
    LinkErrorCallback link_error_cb;
    std::unordered_map<PSM, std::queue<ChannelData>> expected_outbound_conns;

    // LE-only callbacks
    LEConnectionParameterUpdateCallback le_conn_param_cb;
  };

  LinkData* RegisterInternal(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                             bt::LinkType link_type, LinkErrorCallback link_error_callback);

  fbl::RefPtr<testing::FakeChannel> OpenFakeChannel(
      LinkData* link, ChannelId id, ChannelId remote_id,
      ChannelInfo info = ChannelInfo::MakeBasicMode(kDefaultMTU, kDefaultMTU));
  fbl::RefPtr<testing::FakeChannel> OpenFakeFixedChannel(LinkData* link, ChannelId id);

  // Gets the link data for |handle|, creating it if necessary.
  LinkData& GetLinkData(hci_spec::ConnectionHandle handle);
  // Gets the link data for |handle|. Asserts if the link is not connected
  // yet.
  LinkData& ConnectedLinkData(hci_spec::ConnectionHandle handle);

  std::unordered_map<hci_spec::ConnectionHandle, LinkData> links_;
  FakeChannelCallback chan_cb_;
  bool simulate_open_channel_failure_ = false;

  ConnectionParameterUpdateRequestResponder connection_parameter_update_request_responder_;

  std::unordered_map<PSM, ServiceInfo> registered_services_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeL2cap);
};

}  // namespace bt::l2cap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_L2CAP_H_
