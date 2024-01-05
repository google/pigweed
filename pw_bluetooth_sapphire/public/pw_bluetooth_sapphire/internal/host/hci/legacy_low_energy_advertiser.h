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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_advertiser.h"

namespace bt::hci {

class Transport;
class SequentialCommandRunner;

class LegacyLowEnergyAdvertiser final : public LowEnergyAdvertiser {
 public:
  explicit LegacyLowEnergyAdvertiser(hci::Transport::WeakPtr hci)
      : LowEnergyAdvertiser(std::move(hci)) {}
  ~LegacyLowEnergyAdvertiser() override;

  // LowEnergyAdvertiser overrides:
  size_t MaxAdvertisements() const override { return 1; }
  size_t GetSizeLimit() const override {
    return hci_spec::kMaxLEAdvertisingDataLength;
  }
  bool AllowsRandomAddressChange() const override {
    return !starting_ && !IsAdvertising();
  }

  // LegacyLowEnergyAdvertiser supports only a single advertising instance,
  // hence it can report additional errors in the following conditions:
  // 1. If called while a start request is pending, reports kRepeatedAttempts.
  // 2. If called while a stop request is pending, then cancels the stop request
  //    and proceeds with start.
  void StartAdvertising(const DeviceAddress& address,
                        const AdvertisingData& data,
                        const AdvertisingData& scan_rsp,
                        AdvertisingOptions options,
                        ConnectionCallback connect_callback,
                        ResultFunction<> status_callback) override;

  void StopAdvertising() override;

  // If called while a stop request is pending, returns false.
  // If called while a start request is pending, then cancels the start
  // request and proceeds with start.
  // Returns false if called while not advertising.
  // TODO(fxbug.dev/50542): Update documentation.
  void StopAdvertising(const DeviceAddress& address) override;

  void OnIncomingConnection(
      hci_spec::ConnectionHandle handle,
      pw::bluetooth::emboss::ConnectionRole role,
      const DeviceAddress& peer_address,
      const hci_spec::LEConnectionParameters& conn_params) override;

 private:
  EmbossCommandPacket BuildEnablePacket(
      const DeviceAddress& address,
      pw::bluetooth::emboss::GenericEnableParam enable) override;

  CommandChannel::CommandPacketVariant BuildSetAdvertisingParams(
      const DeviceAddress& address,
      pw::bluetooth::emboss::LEAdvertisingType type,
      pw::bluetooth::emboss::LEOwnAddressType own_address_type,
      AdvertisingIntervalRange interval) override;

  CommandChannel::CommandPacketVariant BuildSetAdvertisingData(
      const DeviceAddress& address,
      const AdvertisingData& data,
      AdvFlags flags) override;

  CommandChannel::CommandPacketVariant BuildUnsetAdvertisingData(
      const DeviceAddress& address) override;

  CommandChannel::CommandPacketVariant BuildSetScanResponse(
      const DeviceAddress& address, const AdvertisingData& scan_rsp) override;

  CommandChannel::CommandPacketVariant BuildUnsetScanResponse(
      const DeviceAddress& address) override;

  EmbossCommandPacket BuildRemoveAdvertisingSet(
      const DeviceAddress& address) override;

  // |starting_| is set to true if a start is pending.
  // |staged_params_| are the parameters that will be advertised.
  struct StagedParams {
    DeviceAddress address;
    AdvertisingIntervalRange interval;
    AdvFlags flags;
    AdvertisingData data;
    AdvertisingData scan_rsp;
    ConnectionCallback connect_callback;
    ResultFunction<> result_callback;
  };
  std::optional<StagedParams> staged_params_;
  bool starting_ = false;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyAdvertiser);
};

}  // namespace bt::hci
