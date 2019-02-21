// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/data/socket_factory.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"

namespace btlib {

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
  inline static fbl::RefPtr<FakeDomain> Create() {
    return fbl::AdoptRef(new FakeDomain());
  }

  // Triggers a LE connection parameter update callback on the given link.
  void TriggerLEConnectionParameterUpdate(
      hci::ConnectionHandle handle,
      const hci::LEPreferredConnectionParameters& params);

  // Sets up the expectation that an outbound dynamic channel will be opened
  // on the given link. Each call will expect one dyanmic channel to be
  // created.  If a call to OpenL2capChannel is made without expectation, it
  // will assert.
  // Multiple expectations for the same PSM should be queued in FIFO order.
  void ExpectOutboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                  l2cap::ChannelId id,
                                  l2cap::ChannelId remote_id);

  // Triggers the creation of an inbound dynamic channel on the given link. The
  // channels created will be provided to handlers passed to RegisterService.
  void TriggerInboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                  l2cap::ChannelId id,
                                  l2cap::ChannelId remote_id);

  // Triggers a link error callback on the given link.
  void TriggerLinkError(hci::ConnectionHandle handle);

  // Domain overrides:
  void Initialize() override;
  void ShutDown() override;
  void AddACLConnection(hci::ConnectionHandle handle,
                        hci::Connection::Role role,
                        l2cap::LinkErrorCallback link_error_callback,
                        l2cap::SecurityUpgradeCallback security_callback,
                        async_dispatcher_t* dispatcher) override;
  void AddLEConnection(
      hci::ConnectionHandle handle, hci::Connection::Role role,
      l2cap::LinkErrorCallback link_error_callback,
      l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
      l2cap::LEFixedChannelsCallback channel_callback,
      l2cap::SecurityUpgradeCallback security_callback,
      async_dispatcher_t* dispatcher) override;
  void RemoveConnection(hci::ConnectionHandle handle) override;
  void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                    sm::SecurityProperties security) override;
  void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                        l2cap::ChannelCallback cb,
                        async_dispatcher_t* dispatcher) override;
  void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                        SocketCallback socket_callback,
                        async_dispatcher_t* dispatcher) override;
  void RegisterService(l2cap::PSM psm, l2cap::ChannelCallback channel_callback,
                       async_dispatcher_t* dispatcher) override;
  void RegisterService(l2cap::PSM psm, SocketCallback socket_callback,
                       async_dispatcher_t* dispatcher) override;
  void UnregisterService(l2cap::PSM psm) override;

  // Called when a new channel gets opened. Tests can use this to obtain a
  // reference to all channels.
  using FakeChannelCallback =
      fit::function<void(fbl::RefPtr<l2cap::testing::FakeChannel>)>;
  void set_channel_callback(FakeChannelCallback callback) {
    chan_cb_ = std::move(callback);
  }

 private:
  friend class fbl::RefPtr<FakeDomain>;

  // TODO(armansito): Consider moving the following logic into an internal fake
  // that is L2CAP-specific.
  // Each one of these pairs is a local id and remote id of the channel that
  // will be returned.
  using ChannelIdPair = std::pair<l2cap::ChannelId, l2cap::ChannelId>;
  struct LinkData {
    // Expectations on links can be created before they are connected.
    bool connected;
    hci::ConnectionHandle handle;
    hci::Connection::Role role;
    hci::Connection::LinkType type;

    async_dispatcher_t* dispatcher;

    // Dual-mode callbacks
    l2cap::LinkErrorCallback link_error_cb;
    std::unordered_map<l2cap::PSM, std::queue<ChannelIdPair>>
        expected_outbound_conns;

    // LE-only callbacks
    l2cap::LEConnectionParameterUpdateCallback le_conn_param_cb;
  };

  FakeDomain() = default;
  ~FakeDomain() override = default;

  LinkData* RegisterInternal(hci::ConnectionHandle handle,
                             hci::Connection::Role role,
                             hci::Connection::LinkType link_type,
                             l2cap::LinkErrorCallback link_error_callback,
                             async_dispatcher_t* dispatcher);
  fbl::RefPtr<l2cap::testing::FakeChannel> OpenFakeChannel(
      LinkData* link, l2cap::ChannelId id, l2cap::ChannelId remote_id);
  fbl::RefPtr<l2cap::testing::FakeChannel> OpenFakeFixedChannel(
      LinkData* link, l2cap::ChannelId id);

  // Gets the link data for |handle|, creating it if necessary.
  LinkData& GetLinkData(hci::ConnectionHandle handle);
  // Gets the link data for |handle|. Asserts if the link is not connected
  // yet.
  LinkData& ConnectedLinkData(hci::ConnectionHandle handle);

  bool initialized_ = false;
  std::unordered_map<hci::ConnectionHandle, LinkData> links_;
  FakeChannelCallback chan_cb_;

  using ChannelDelivery =
      std::pair<l2cap::ChannelCallback, async_dispatcher_t*>;
  std::unordered_map<l2cap::PSM, ChannelDelivery> inbound_conn_cbs_;

  // Makes sockets for RegisterService
  internal::SocketFactory<l2cap::Channel> socket_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(FakeDomain);
};

}  // namespace testing
}  // namespace data
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_FAKE_DOMAIN_H_
