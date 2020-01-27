// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_domain.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {

using l2cap::testing::FakeChannel;

namespace data {
namespace testing {

bool FakeDomain::IsLinkConnected(hci::ConnectionHandle handle) const {
  auto link_iter = links_.find(handle);
  if (link_iter == links_.end()) {
    return false;
  }
  return link_iter->second.connected;
}

void FakeDomain::TriggerLEConnectionParameterUpdate(
    hci::ConnectionHandle handle, const hci::LEPreferredConnectionParameters& params) {
  ZX_DEBUG_ASSERT(initialized_);

  LinkData& link_data = ConnectedLinkData(handle);
  async::PostTask(link_data.dispatcher,
                  [params, cb = link_data.le_conn_param_cb.share()] { cb(params); });
}

void FakeDomain::ExpectOutboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                            l2cap::ChannelId id, l2cap::ChannelId remote_id,
                                            l2cap::ChannelParameters params) {
  ZX_DEBUG_ASSERT(initialized_);
  LinkData& link_data = GetLinkData(handle);
  ChannelData chan_data;
  chan_data.local_id = id;
  chan_data.remote_id = remote_id;
  chan_data.params = params;
  link_data.expected_outbound_conns[psm].push(chan_data);
}

void FakeDomain::TriggerInboundL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                            l2cap::ChannelId id, l2cap::ChannelId remote_id,
                                            uint16_t tx_mtu) {
  ZX_DEBUG_ASSERT(initialized_);

  LinkData& link_data = ConnectedLinkData(handle);
  auto cb_iter = registered_services_.find(psm);
  ZX_DEBUG_ASSERT_MSG(cb_iter != registered_services_.end(), "no service registered for PSM %#.4x",
                      psm);

  l2cap::ChannelCallback& cb = cb_iter->second.channel_cb;
  auto chan_params = cb_iter->second.channel_params;
  auto mode = chan_params.mode.value_or(l2cap::ChannelMode::kBasic);
  auto rx_mtu = chan_params.max_sdu_size.value_or(l2cap::kDefaultMTU);

  auto chan = OpenFakeChannel(&link_data, id, remote_id, l2cap::ChannelInfo(mode, rx_mtu, tx_mtu));
  cb(std::move(chan));
}

void FakeDomain::TriggerLinkError(hci::ConnectionHandle handle) {
  ZX_DEBUG_ASSERT(initialized_);

  LinkData& link_data = ConnectedLinkData(handle);
  async::PostTask(link_data.dispatcher, [cb = link_data.link_error_cb.share()] { cb(); });
}

void FakeDomain::Initialize() { initialized_ = true; }

void FakeDomain::ShutDown() { initialized_ = false; }

void FakeDomain::AddACLConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                                  l2cap::LinkErrorCallback link_error_cb,
                                  l2cap::SecurityUpgradeCallback security_cb,
                                  async_dispatcher_t* dispatcher) {
  if (!initialized_)
    return;

  RegisterInternal(handle, role, hci::Connection::LinkType::kACL, std::move(link_error_cb),
                   dispatcher);
}

void FakeDomain::AddLEConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                                 l2cap::LinkErrorCallback link_error_cb,
                                 l2cap::LEConnectionParameterUpdateCallback conn_param_cb,
                                 l2cap::LEFixedChannelsCallback channel_cb,
                                 l2cap::SecurityUpgradeCallback security_cb,
                                 async_dispatcher_t* dispatcher) {
  if (!initialized_)
    return;

  LinkData* data = RegisterInternal(handle, role, hci::Connection::LinkType::kLE,
                                    std::move(link_error_cb), dispatcher);
  data->le_conn_param_cb = std::move(conn_param_cb);

  // Open the ATT and SMP fixed channels.
  auto att = OpenFakeFixedChannel(data, l2cap::kATTChannelId);
  auto smp = OpenFakeFixedChannel(data, l2cap::kLESMPChannelId);
  async::PostTask(dispatcher,
                  [att = std::move(att), smp = std::move(smp),
                   cb = std::move(channel_cb)]() mutable { cb(std::move(att), std::move(smp)); });
}

void FakeDomain::RemoveConnection(hci::ConnectionHandle handle) { links_.erase(handle); }

void FakeDomain::AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                              sm::SecurityProperties security) {
  // TODO(armansito): implement
}

void FakeDomain::OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                  l2cap::ChannelParameters params, l2cap::ChannelCallback cb,
                                  async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(initialized_);

  LinkData& link_data = ConnectedLinkData(handle);
  auto psm_it = link_data.expected_outbound_conns.find(psm);

  ZX_DEBUG_ASSERT_MSG(psm_it != link_data.expected_outbound_conns.end() && !psm_it->second.empty(),
                      "Unexpected outgoing L2CAP connection (PSM %#.4x)", psm);

  auto chan_data = psm_it->second.front();
  psm_it->second.pop();

  auto mode = params.mode.value_or(l2cap::ChannelMode::kBasic);
  auto rx_mtu = params.max_sdu_size.value_or(l2cap::kMaxMTU);

  ZX_ASSERT_MSG(chan_data.params == params,
                "Didn't receive expected L2CAP channel parameters (expected: %s, found: %s)",
                bt_str(chan_data.params), bt_str(params));

  auto chan = OpenFakeChannel(&link_data, chan_data.local_id, chan_data.remote_id,
                              l2cap::ChannelInfo(mode, rx_mtu, l2cap::kDefaultMTU));

  async::PostTask(dispatcher,
                  [cb = std::move(cb), chan = std::move(chan)]() { cb(std::move(chan)); });
}

void FakeDomain::OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                  l2cap::ChannelParameters params, SocketCallback socket_callback,
                                  async_dispatcher_t* cb_dispatcher) {
  ZX_DEBUG_ASSERT(cb_dispatcher);
  OpenL2capChannel(
      handle, psm, params,
      [this, cb = std::move(socket_callback), cb_dispatcher](auto channel) mutable {
        zx::socket s = socket_factory_.MakeSocketForChannel(channel);
        auto chan_info = channel ? std::optional(channel->info()) : std::nullopt;
        l2cap::ChannelSocket chan_sock(std::move(s), chan_info);

        // Called every time the service is connected, cb must be shared.
        async::PostTask(cb_dispatcher, [chan_sock = std::move(chan_sock), cb = cb.share(),
                                        handle = channel->link_handle()]() mutable {
          cb(std::move(chan_sock), handle);
        });
      },
      async_get_default_dispatcher());
}

void FakeDomain::RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                                 l2cap::ChannelCallback channel_callback,
                                 async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(initialized_);
  ZX_DEBUG_ASSERT(registered_services_.count(psm) == 0);

  // capture |dispatcher| here so it doesn't need to be stored with ServiceInfo
  auto service_cb = [dispatcher, chan_cb = std::move(channel_callback)](auto chan) mutable {
    async::PostTask(dispatcher,
                    [cb = chan_cb.share(), chan = std::move(chan)]() { cb(std::move(chan)); });
  };
  registered_services_.emplace(psm, ServiceInfo(params, std::move(service_cb)));
}

void FakeDomain::RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                                 SocketCallback socket_callback,
                                 async_dispatcher_t* cb_dispatcher) {
  RegisterService(
      psm, params,
      [this, cb = std::move(socket_callback), cb_dispatcher](auto channel) mutable {
        zx::socket s = socket_factory_.MakeSocketForChannel(channel);
        auto chan_info = channel ? std::optional(channel->info()) : std::nullopt;
        l2cap::ChannelSocket chan_sock(std::move(s), chan_info);

        // Called every time the service is connected, cb must be shared.
        async::PostTask(cb_dispatcher, [chan_sock = std::move(chan_sock), cb = cb.share(),
                                        handle = channel->link_handle()]() mutable {
          cb(std::move(chan_sock), handle);
        });
      },
      async_get_default_dispatcher());
}

void FakeDomain::UnregisterService(l2cap::PSM psm) {
  ZX_DEBUG_ASSERT(initialized_);

  registered_services_.erase(psm);
}

FakeDomain::~FakeDomain() {
  for (auto& link_it : links_) {
    for (auto& psm_it : link_it.second.expected_outbound_conns) {
      ZX_DEBUG_ASSERT_MSG(psm_it.second.empty(), "didn't receive expected connection on PSM %#.4x",
                          psm_it.first);
    }
  }
}

FakeDomain::LinkData* FakeDomain::RegisterInternal(hci::ConnectionHandle handle,
                                                   hci::Connection::Role role,
                                                   hci::Connection::LinkType link_type,
                                                   l2cap::LinkErrorCallback link_error_cb,
                                                   async_dispatcher_t* dispatcher) {
  auto& data = GetLinkData(handle);
  ZX_DEBUG_ASSERT_MSG(!data.connected, "connection handle re-used (handle: %#.4x)", handle);

  data.connected = true;
  data.role = role;
  data.type = link_type;
  data.link_error_cb = std::move(link_error_cb);
  data.dispatcher = dispatcher;

  return &data;
}

fbl::RefPtr<FakeChannel> FakeDomain::OpenFakeChannel(LinkData* link, l2cap::ChannelId id,
                                                     l2cap::ChannelId remote_id,
                                                     l2cap::ChannelInfo info) {
  fbl::RefPtr<FakeChannel> chan;
  if (!simulate_open_channel_failure_) {
    chan = fbl::AdoptRef(new FakeChannel(id, remote_id, link->handle, link->type, info));
    chan->SetLinkErrorCallback(link->link_error_cb.share(), link->dispatcher);
  }

  if (chan_cb_) {
    chan_cb_(chan);
  }

  return chan;
}

fbl::RefPtr<FakeChannel> FakeDomain::OpenFakeFixedChannel(LinkData* link, l2cap::ChannelId id) {
  return OpenFakeChannel(link, id, id);
}

FakeDomain::LinkData& FakeDomain::GetLinkData(hci::ConnectionHandle handle) {
  auto [it, inserted] = links_.try_emplace(handle);
  auto& data = it->second;
  if (inserted) {
    data.connected = false;
    data.handle = handle;
  }
  return data;
}

FakeDomain::LinkData& FakeDomain::ConnectedLinkData(hci::ConnectionHandle handle) {
  auto link_iter = links_.find(handle);
  ZX_DEBUG_ASSERT_MSG(link_iter != links_.end(), "fake link not found (handle: %#.4x)", handle);
  ZX_DEBUG_ASSERT_MSG(link_iter->second.connected, "fake link not connected yet (handle: %#.4x)",
                      handle);
  return link_iter->second;
}

}  // namespace testing
}  // namespace data
}  // namespace bt
