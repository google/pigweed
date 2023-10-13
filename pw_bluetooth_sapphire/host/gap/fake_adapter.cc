// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_adapter.h"

#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"

namespace bt::gap::testing {

FakeAdapter::FakeAdapter(pw::async::Dispatcher& pw_dispatcher)
    : init_state_(InitState::kNotInitialized),
      fake_le_(std::make_unique<FakeLowEnergy>(this)),
      fake_bredr_(std::make_unique<FakeBrEdr>()),
      heap_dispatcher_(pw_dispatcher),
      peer_cache_(pw_dispatcher),
      weak_self_(this) {}

bool FakeAdapter::Initialize(InitializeCallback callback, fit::closure transport_closed_callback) {
  init_state_ = InitState::kInitializing;
  heap_dispatcher_.Post(
      [this, cb = std::move(callback)](pw::async::Context /*ctx*/, pw::Status status) mutable {
        if (status.ok()) {
          init_state_ = InitState::kInitialized;
          cb(/*success=*/true);
        }
      });
  return true;
}

void FakeAdapter::ShutDown() { init_state_ = InitState::kNotInitialized; }

FakeAdapter::FakeBrEdr::~FakeBrEdr() {
  for (auto& chan : channels_) {
    chan.second->Close();
  }
}

void FakeAdapter::FakeBrEdr::OpenL2capChannel(PeerId peer_id, l2cap::Psm psm,
                                              BrEdrSecurityRequirements security_requirements,
                                              l2cap::ChannelParameters params,
                                              l2cap::ChannelCallback cb) {
  l2cap::ChannelInfo info(params.mode.value_or(l2cap::RetransmissionAndFlowControlMode::kBasic),
                          params.max_rx_sdu_size.value_or(l2cap::kDefaultMTU),
                          /*max_tx_sdu_size=*/l2cap::kDefaultMTU, /*n_frames_in_tx_window=*/0,
                          /*max_transmissions=*/0, /*max_tx_pdu_payload_size=*/0, psm,
                          params.flush_timeout);
  l2cap::ChannelId local_id = next_channel_id_++;
  auto channel = std::make_unique<l2cap::testing::FakeChannel>(
      /*id=*/local_id, /*remote_id=*/l2cap::kFirstDynamicChannelId,
      /*handle=*/1, bt::LinkType::kACL, info);
  l2cap::testing::FakeChannel::WeakPtr weak_fake_channel = channel->AsWeakPtr();
  l2cap::Channel::WeakPtr weak_channel = channel->GetWeakPtr();
  channels_.emplace(local_id, std::move(channel));
  if (channel_cb_) {
    channel_cb_(weak_fake_channel);
  }
  cb(weak_channel);
}

void FakeAdapter::FakeLowEnergy::UpdateRandomAddress(DeviceAddress& address) {
  random_ = address;
  // Notify the callback about the change in address.
  if (address_changed_callback_) {
    address_changed_callback_();
  }
}

void FakeAdapter::FakeLowEnergy::Connect(PeerId peer_id, ConnectionResultCallback callback,
                                         LowEnergyConnectionOptions connection_options) {
  connections_[peer_id] = Connection{peer_id, connection_options};

  auto bondable_cb = [connection_options]() { return connection_options.bondable_mode; };
  auto security_cb = []() { return sm::SecurityProperties(); };
  auto handle = std::make_unique<LowEnergyConnectionHandle>(
      peer_id, /*handle=*/1,
      /*release_cb=*/[](auto) {}, std::move(bondable_cb), std::move(security_cb));
  callback(fit::ok(std::move(handle)));
}

bool FakeAdapter::FakeLowEnergy::Disconnect(PeerId peer_id) { return connections_.erase(peer_id); }

void FakeAdapter::FakeLowEnergy::StartAdvertising(
    AdvertisingData data, AdvertisingData scan_rsp, AdvertisingInterval interval, bool anonymous,
    bool include_tx_power_level, std::optional<ConnectableAdvertisingParameters> connectable,
    AdvertisingStatusCallback status_callback) {
  // status_callback is currently not called because its parameters can only be constructed by
  // LowEnergyAdvertisingManager.

  RegisteredAdvertisement adv{.data = std::move(data),
                              .scan_rsp = std::move(scan_rsp),
                              .connectable = std::move(connectable),
                              .interval = interval,
                              .anonymous = anonymous,
                              .include_tx_power_level = include_tx_power_level};
  AdvertisementId adv_id = next_advertisement_id_;
  next_advertisement_id_ = AdvertisementId(next_advertisement_id_.value() + 1);
  advertisements_.emplace(adv_id, std::move(adv));
}

void FakeAdapter::FakeLowEnergy::EnablePrivacy(bool enabled) {
  privacy_enabled_ = enabled;
  if (!enabled && random_.has_value()) {
    random_.reset();
    if (address_changed_callback_) {
      address_changed_callback_();
    }
  }
}

FakeAdapter::FakeBrEdr::RegistrationHandle FakeAdapter::FakeBrEdr::RegisterService(
    std::vector<sdp::ServiceRecord> records, l2cap::ChannelParameters chan_params,
    ServiceConnectCallback conn_cb) {
  auto handle = next_service_handle_++;
  registered_services_.emplace(
      handle, RegisteredService{std::move(records), chan_params, std::move(conn_cb)});
  return handle;
}

void FakeAdapter::SetLocalName(std::string name, hci::ResultFunction<> callback) {
  local_name_ = name;
  callback(fit::ok());
}

void FakeAdapter::SetDeviceClass(DeviceClass dev_class, hci::ResultFunction<> callback) {
  device_class_ = dev_class;
  callback(fit::ok());
}

BrEdrConnectionManager::SearchId FakeAdapter::FakeBrEdr::AddServiceSearch(
    const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes, SearchCallback callback) {
  auto handle = next_search_handle_++;
  registered_searches_.emplace(handle,
                               RegisteredSearch{uuid, std::move(attributes), std::move(callback)});
  return SearchId(handle);
}

void FakeAdapter::FakeBrEdr::TriggerServiceFound(
    PeerId peer_id, UUID uuid, std::map<sdp::AttributeId, sdp::DataElement> attributes) {
  for (auto it = registered_searches_.begin(); it != registered_searches_.end(); it++) {
    if (it->second.uuid == uuid) {
      it->second.callback(peer_id, attributes);
    }
  }
}

}  // namespace bt::gap::testing
