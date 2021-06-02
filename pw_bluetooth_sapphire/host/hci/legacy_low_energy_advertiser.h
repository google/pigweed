// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_ADVERTISER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_ADVERTISER_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_advertiser.h"

namespace bt::hci {

class Transport;
class SequentialCommandRunner;

class LegacyLowEnergyAdvertiser final : public LowEnergyAdvertiser {
 public:
  explicit LegacyLowEnergyAdvertiser(fxl::WeakPtr<Transport> hci)
      : LowEnergyAdvertiser(std::move(hci)) {}
  ~LegacyLowEnergyAdvertiser() override { StopAdvertising(); }

  // LowEnergyAdvertiser overrides:
  size_t GetSizeLimit() override { return kMaxLEAdvertisingDataLength; }
  size_t GetMaxAdvertisements() const override { return 1; }
  bool AllowsRandomAddressChange() const override { return !starting_ && !IsAdvertising(); }

  // LegacyLowEnergyAdvertiser supports only a single advertising instance,
  // hence it can report additional errors in the following conditions:
  // 1. If called while a start request is pending, reports kRepeatedAttempts.
  // 2. If called while a stop request is pending, then cancels the stop request
  //    and proceeds with start.
  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions adv_options,
                        ConnectionCallback connect_callback,
                        StatusCallback status_callback) override;

  bool StopAdvertising() override;

  // If called while a stop request is pending, returns false.
  // If called while a start request is pending, then cancels the start
  // request and proceeds with start.
  // Returns false if called while not advertising.
  // TODO(fxbug.dev/50542): Update documentation.
  bool StopAdvertising(const DeviceAddress& address) override;

  void OnIncomingConnection(ConnectionHandle handle, Connection::Role role,
                            std::optional<DeviceAddress> opt_local_address,
                            const DeviceAddress& peer_address,
                            const LEConnectionParameters& conn_params) override;

 private:
  std::unique_ptr<CommandPacket> BuildEnablePacket(GenericEnableParam enable) override;

  std::unique_ptr<CommandPacket> BuildSetAdvertisingParams(
      LEAdvertisingType type, LEOwnAddressType own_address_type,
      AdvertisingIntervalRange interval) override;

  std::unique_ptr<CommandPacket> BuildSetAdvertisingData(const AdvertisingData& data,
                                                         AdvFlags flags) override;

  std::unique_ptr<CommandPacket> BuildUnsetAdvertisingData(const DeviceAddress& address) override;

  std::unique_ptr<CommandPacket> BuildSetScanResponse(const AdvertisingData& scan_rsp) override;

  std::unique_ptr<CommandPacket> BuildUnsetScanResponse(const DeviceAddress& address) override;

  // |starting_| is set to true if a start is pending.
  // |staged_params_| are the parameters that will be advertised.
  struct StagedParams {
    DeviceAddress address;
    AdvertisingIntervalRange interval;
    AdvFlags flags;
    AdvertisingData data;
    AdvertisingData scan_rsp;
    ConnectionCallback connect_callback;
    StatusCallback status_callback;
  };
  std::optional<StagedParams> staged_params_;
  bool starting_ = false;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyAdvertiser);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_ADVERTISER_H_
