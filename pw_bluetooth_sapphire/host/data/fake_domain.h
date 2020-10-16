// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"

namespace bt {

namespace l2cap {
namespace testing {
class FakeChannel;
}  // namespace testing
}  // namespace l2cap

namespace data {
namespace testing {

// This is a fake version of the Domain class that can be injected into other
// layers for unit testing.
class FakeDomain final : public Domain {
 public:
  inline static fbl::RefPtr<FakeDomain> Create() { return fbl::AdoptRef(new FakeDomain()); }

  void AttachInspect(inspect::Node& parent, std::string name) override {}

  // Returns true if and only if a link identified by |handle| has been added and connected.
  [[nodiscard]] bool IsLinkConnected(hci::ConnectionHandle handle) const;

  // Triggers a LE connection parameter update callback on the given link.
  void TriggerLEConnectionParameterUpdate(hci::ConnectionHandle handle,
                                          const hci::LEPreferredConnectionParameters& params);

  // Sets up the expectation that an outbound dynamic channel will be opened
  // on the given link. Each call will expect one dyanmic channel to be
  // created.  If a call to OpenL2capChannel is made without expectation, it
  // will assert.
  // Multiple expectations for the same PSM should be queued in FIFO order.
  void ExpectOutboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm, l2cap::ChannelId id,
                                  l2cap::ChannelId remote_id, l2cap::ChannelParameters params);

  // Triggers the creation of an inbound dynamic channel on the given link. The
  // channels created will be provided to handlers passed to RegisterService.
  // Returns false if unable to create the channel.
  bool TriggerInboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm, l2cap::ChannelId id,
                                  l2cap::ChannelId remote_id, uint16_t tx_mtu = l2cap::kDefaultMTU);

  // Triggers a link error callback on the given link.
  void TriggerLinkError(hci::ConnectionHandle handle);

  // Domain overrides:
  void AddACLConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                        l2cap::LinkErrorCallback link_error_callback,
                        l2cap::SecurityUpgradeCallback security_callback) override;
  LEFixedChannels AddLEConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                                  l2cap::LinkErrorCallback link_error_callback,
                                  l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
                                  l2cap::SecurityUpgradeCallback security_callback) override;
  void RemoveConnection(hci::ConnectionHandle handle) override;
  void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                    sm::SecurityProperties security) override;

  // Immediately posts accept on |dispatcher|.
  void RequestConnectionParameterUpdate(
      hci::ConnectionHandle handle, hci::LEPreferredConnectionParameters params,
      l2cap::ConnectionParameterUpdateRequestCallback request_cb) override;
  void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                        l2cap::ChannelParameters params, l2cap::ChannelCallback cb) override;
  void RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                       l2cap::ChannelCallback channel_callback) override;
  void UnregisterService(l2cap::PSM psm) override;

  // Called when a new channel gets opened. Tests can use this to obtain a
  // reference to all channels.
  using FakeChannelCallback = fit::function<void(fbl::RefPtr<l2cap::testing::FakeChannel>)>;
  void set_channel_callback(FakeChannelCallback callback) { chan_cb_ = std::move(callback); }
  void set_simulate_open_channel_failure(bool simulate_failure) {
    simulate_open_channel_failure_ = simulate_failure;
  }

  // Called when RequestConnectionParameterUpdate is called. |request_cb| will be called with return
  // value. Defaults to returning true if not set.
  using ConnectionParameterUpdateRequestResponder =
      fit::function<bool(hci::ConnectionHandle, hci::LEPreferredConnectionParameters)>;
  void set_connection_parameter_update_request_responder(
      ConnectionParameterUpdateRequestResponder responder) {
    connection_parameter_update_request_responder_ = std::move(responder);
  }

 private:
  friend class fbl::RefPtr<FakeDomain>;

  // TODO(armansito): Consider moving the following logic into an internal fake
  // that is L2CAP-specific.
  struct ChannelData {
    l2cap::ChannelId local_id;
    l2cap::ChannelId remote_id;
    l2cap::ChannelParameters params;
  };
  struct LinkData {
    // Expectations on links can be created before they are connected.
    bool connected;
    hci::ConnectionHandle handle;
    hci::Connection::Role role;
    hci::Connection::LinkType type;

    async_dispatcher_t* dispatcher;

    // Dual-mode callbacks
    l2cap::LinkErrorCallback link_error_cb;
    std::unordered_map<l2cap::PSM, std::queue<ChannelData>> expected_outbound_conns;

    // LE-only callbacks
    l2cap::LEConnectionParameterUpdateCallback le_conn_param_cb;
  };

  FakeDomain() = default;
  ~FakeDomain() override;

  LinkData* RegisterInternal(hci::ConnectionHandle handle, hci::Connection::Role role,
                             hci::Connection::LinkType link_type,
                             l2cap::LinkErrorCallback link_error_callback);

  fbl::RefPtr<l2cap::testing::FakeChannel> OpenFakeChannel(
      LinkData* link, l2cap::ChannelId id, l2cap::ChannelId remote_id,
      l2cap::ChannelInfo info = l2cap::ChannelInfo::MakeBasicMode(l2cap::kDefaultMTU,
                                                                  l2cap::kDefaultMTU));
  fbl::RefPtr<l2cap::testing::FakeChannel> OpenFakeFixedChannel(LinkData* link,
                                                                l2cap::ChannelId id);

  // Gets the link data for |handle|, creating it if necessary.
  LinkData& GetLinkData(hci::ConnectionHandle handle);
  // Gets the link data for |handle|. Asserts if the link is not connected
  // yet.
  LinkData& ConnectedLinkData(hci::ConnectionHandle handle);

  std::unordered_map<hci::ConnectionHandle, LinkData> links_;
  FakeChannelCallback chan_cb_;
  bool simulate_open_channel_failure_ = false;

  ConnectionParameterUpdateRequestResponder connection_parameter_update_request_responder_;

  using ServiceInfo = l2cap::ServiceInfo<l2cap::ChannelCallback>;
  std::unordered_map<l2cap::PSM, ServiceInfo> registered_services_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeDomain);
};

}  // namespace testing
}  // namespace data
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_
