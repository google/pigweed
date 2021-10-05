// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_adapter.h"

#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"

namespace bt::gap::testing {

FakeAdapter::FakeAdapter()
    : init_state_(InitState::kNotInitialized),
      fake_le_(std::make_unique<FakeLowEnergy>(this)),
      fake_bredr_(std::make_unique<FakeBrEdr>()),
      weak_ptr_factory_(this) {}

bool FakeAdapter::Initialize(InitializeCallback callback, fit::closure transport_closed_callback) {
  init_state_ = InitState::kInitializing;
  async::PostTask(async_get_default_dispatcher(), [this, cb = std::move(callback)] {
    init_state_ = InitState::kInitialized;
    cb(/*success=*/true);
  });
  return true;
}

void FakeAdapter::ShutDown() { init_state_ = InitState::kNotInitialized; }

void FakeAdapter::FakeBrEdr::OpenL2capChannel(PeerId peer_id, l2cap::PSM psm,
                                              BrEdrSecurityRequirements security_requirements,
                                              l2cap::ChannelParameters params,
                                              l2cap::ChannelCallback cb) {
  l2cap::ChannelInfo info(params.mode.value_or(l2cap::ChannelMode::kBasic),
                          params.max_rx_sdu_size.value_or(l2cap::kDefaultMTU),
                          /*max_tx_sdu_size=*/l2cap::kDefaultMTU, /*n_frames_in_tx_window=*/0,
                          /*max_transmissions=*/0, /*max_tx_pdu_payload_size=*/0, psm,
                          params.flush_timeout);
  auto channel = fbl::AdoptRef(new l2cap::testing::FakeChannel(
      /*id=*/l2cap::kFirstDynamicChannelId, /*remote_id=*/l2cap::kFirstDynamicChannelId,
      /*handle=*/1, bt::LinkType::kACL, info));
  if (channel_cb_) {
    channel_cb_(channel);
  }
  cb(channel);
}

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

FakeAdapter::FakeBrEdr::RegistrationHandle FakeAdapter::FakeBrEdr::RegisterService(
    std::vector<sdp::ServiceRecord> records, l2cap::ChannelParameters chan_params,
    ServiceConnectCallback conn_cb) {
  auto handle = next_registration_handle_++;
  registered_services_.emplace(
      handle, RegisteredService{std::move(records), chan_params, std::move(conn_cb)});
  return handle;
}

void FakeAdapter::SetLocalName(std::string name, hci::StatusCallback callback) {
  local_name_ = name;
  callback(hci::Status());
}

void FakeAdapter::SetDeviceClass(DeviceClass dev_class, hci::StatusCallback callback) {
  device_class_ = dev_class;
  callback(hci::Status());
}

}  // namespace bt::gap::testing
