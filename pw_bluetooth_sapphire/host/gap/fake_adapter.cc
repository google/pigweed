// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/gap/fake_adapter.h"

#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"
#include "pw_bluetooth_sapphire/internal/host/transport/link_type.h"

namespace bt::gap::testing {

FakeAdapter::FakeAdapter(pw::async::Dispatcher& pw_dispatcher)
    : init_state_(InitState::kNotInitialized),
      fake_le_(std::make_unique<FakeLowEnergy>(this)),
      fake_bredr_(std::make_unique<FakeBrEdr>()),
      pw_dispatcher_(pw_dispatcher),
      heap_dispatcher_(pw_dispatcher),
      peer_cache_(pw_dispatcher),
      weak_self_(this) {}

bool FakeAdapter::Initialize(InitializeCallback callback, fit::closure) {
  init_state_ = InitState::kInitializing;
  (void)heap_dispatcher_.Post(
      [this, cb = std::move(callback)](pw::async::Context /*ctx*/,
                                       pw::Status status) mutable {
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

void FakeAdapter::FakeBrEdr::OpenL2capChannel(PeerId,
                                              l2cap::Psm psm,
                                              BrEdrSecurityRequirements,
                                              l2cap::ChannelParameters params,
                                              l2cap::ChannelCallback cb) {
  l2cap::ChannelInfo info(
      params.mode.value_or(l2cap::RetransmissionAndFlowControlMode::kBasic),
      params.max_rx_sdu_size.value_or(l2cap::kDefaultMTU),
      /*max_tx_sdu_size=*/l2cap::kDefaultMTU,
      /*n_frames_in_tx_window=*/0,
      /*max_transmissions=*/0,
      /*max_tx_pdu_payload_size=*/0,
      psm,
      params.flush_timeout);
  l2cap::ChannelId local_id = next_channel_id_++;
  auto channel = std::make_unique<l2cap::testing::FakeChannel>(
      /*id=*/local_id,
      /*remote_id=*/l2cap::kFirstDynamicChannelId,
      /*handle=*/1,
      bt::LinkType::kACL,
      info);
  l2cap::testing::FakeChannel::WeakPtr weak_fake_channel = channel->AsWeakPtr();
  l2cap::Channel::WeakPtr weak_channel = channel->GetWeakPtr();
  channels_.emplace(local_id, std::move(channel));
  if (channel_cb_) {
    channel_cb_(weak_fake_channel);
  }
  cb(weak_channel);
}

void FakeAdapter::FakeLowEnergy::UpdateRandomAddress(DeviceAddress& address) {
  fake_address_delegate_.UpdateRandomAddress(address);
}

void FakeAdapter::FakeLowEnergy::Connect(
    PeerId peer_id,
    ConnectionResultCallback callback,
    LowEnergyConnectionOptions connection_options) {
  connections_[peer_id] = Connection{peer_id, connection_options};

  auto accept_cis_cb = [](iso::CigCisIdentifier, iso::CisEstablishedCallback) {
    return iso::AcceptCisStatus::kSuccess;
  };
  auto bondable_cb = [connection_options]() {
    return connection_options.bondable_mode;
  };
  auto security_cb = []() { return sm::SecurityProperties(); };
  auto role_cb = []() {
    return pw::bluetooth::emboss::ConnectionRole::CENTRAL;
  };
  auto handle = std::make_unique<LowEnergyConnectionHandle>(
      peer_id,
      /*handle=*/1,
      /*release_cb=*/[](auto) {},
      std::move(accept_cis_cb),
      std::move(bondable_cb),
      std::move(security_cb),
      std::move(role_cb));
  callback(fit::ok(std::move(handle)));
}

bool FakeAdapter::FakeLowEnergy::Disconnect(PeerId peer_id) {
  return connections_.erase(peer_id);
}

void FakeAdapter::FakeLowEnergy::OpenL2capChannel(
    PeerId,
    l2cap::Psm psm,
    l2cap::ChannelParameters params,
    sm::SecurityLevel,
    l2cap::ChannelCallback cb) {
  l2cap::ChannelInfo info(
      params.mode.value_or(
          l2cap::CreditBasedFlowControlMode::kLeCreditBasedFlowControl),
      params.max_rx_sdu_size.value_or(l2cap::kDefaultMTU),
      /*max_tx_sdu_size=*/l2cap::kDefaultMTU,
      /*n_frames_in_tx_window=*/0,
      /*max_transmissions=*/0,
      /*max_tx_pdu_payload_size=*/0,
      psm,
      params.flush_timeout);

  l2cap::ChannelId local_id = next_channel_id_++;
  auto channel = std::make_unique<l2cap::testing::FakeChannel>(
      /*id=*/local_id,
      /*remote_id=*/l2cap::kFirstDynamicChannelId,
      /*handle=*/1,
      bt::LinkType::kLE,
      info);

  l2cap::Channel::WeakPtr weak_channel = channel->GetWeakPtr();
  channels_.emplace(local_id, std::move(channel));
  cb(weak_channel);
}

void FakeAdapter::FakeLowEnergy::StartAdvertising(
    AdvertisingData,
    AdvertisingData,
    AdvertisingInterval,
    bool,
    bool,
    bool include_tx_power_level,
    std::optional<ConnectableAdvertisingParameters>,
    std::optional<DeviceAddress::Type> address_type,
    AdvertisingStatusCallback status_callback) {
  fake_address_delegate_.EnsureLocalAddress(
      address_type,
      [this,
       include_tx_power_level,
       status_callback = std::move(status_callback)](
          fit::result<HostError, DeviceAddress> result) {
        if (result.is_error()) {
          status_callback(AdvertisementInstance(),
                          fit::error(result.error_value()));
          return;
        }

        RegisteredAdvertisement adv{
            .include_tx_power_level = include_tx_power_level,
            .addr_type = result.value().type()};

        AdvertisementId adv_id = next_advertisement_id_;
        next_advertisement_id_ =
            AdvertisementId(next_advertisement_id_.value() + 1);
        advertisements_.emplace(adv_id, std::move(adv));
      });
}

void FakeAdapter::FakeLowEnergy::EnablePrivacy(bool enabled) {
  fake_address_delegate_.EnablePrivacy(enabled);
}

FakeAdapter::FakeBrEdr::RegistrationHandle
FakeAdapter::FakeBrEdr::RegisterService(std::vector<sdp::ServiceRecord> records,
                                        l2cap::ChannelParameters chan_params,
                                        ServiceConnectCallback conn_cb) {
  auto handle = next_service_handle_++;
  registered_services_.emplace(
      handle,
      RegisteredService{std::move(records), chan_params, std::move(conn_cb)});
  return handle;
}

bool FakeAdapter::FakeBrEdr::UnregisterService(RegistrationHandle handle) {
  return registered_services_.erase(handle);
}

void FakeAdapter::SetLocalName(std::string name,
                               hci::ResultFunction<> callback) {
  local_name_ = name;
  callback(fit::ok());
}

void FakeAdapter::SetDeviceClass(DeviceClass dev_class,
                                 hci::ResultFunction<> callback) {
  device_class_ = dev_class;
  callback(fit::ok());
}

void FakeAdapter::GetSupportedDelayRange(
    const bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>&,
    pw::bluetooth::emboss::LogicalTransportType,
    pw::bluetooth::emboss::DataPathDirection,
    const std::optional<std::vector<uint8_t>>&,
    GetSupportedDelayRangeCallback cb) {
  cb(PW_STATUS_OK,
     0,
     pw::bluetooth::emboss::
         ReadLocalSupportedControllerDelayCommandCompleteEvent::
             max_delay_usecs());
}

BrEdrConnectionManager::SearchId FakeAdapter::FakeBrEdr::AddServiceSearch(
    const UUID& uuid,
    std::unordered_set<sdp::AttributeId> attributes,
    SearchCallback callback) {
  auto handle = next_search_handle_++;
  registered_searches_.emplace(
      handle,
      RegisteredSearch{uuid, std::move(attributes), std::move(callback)});
  return SearchId(handle);
}

void FakeAdapter::FakeBrEdr::TriggerServiceFound(
    PeerId peer_id,
    UUID uuid,
    std::map<sdp::AttributeId, sdp::DataElement> attributes) {
  for (auto it = registered_searches_.begin(); it != registered_searches_.end();
       it++) {
    if (it->second.uuid == uuid) {
      it->second.callback(peer_id, attributes);
    }
  }
}

}  // namespace bt::gap::testing
