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
  ~LegacyLowEnergyAdvertiser() override;

  // LowEnergyAdvertiser overrides:
  size_t MaxAdvertisements() const override { return 1; }
  size_t GetSizeLimit() const override { return hci_spec::kMaxLEAdvertisingDataLength; }
  bool AllowsRandomAddressChange() const override { return !starting_ && !IsAdvertising(); }

  // LegacyLowEnergyAdvertiser supports only a single advertising instance,
  // hence it can report additional errors in the following conditions:
  // 1. If called while a start request is pending, reports kRepeatedAttempts.
  // 2. If called while a stop request is pending, then cancels the stop request
  //    and proceeds with start.
  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions options,
                        ConnectionCallback connect_callback,
                        ResultFunction<> status_callback) override;

  void StopAdvertising() override;

  // If called while a stop request is pending, returns false.
  // If called while a start request is pending, then cancels the start
  // request and proceeds with start.
  // Returns false if called while not advertising.
  // TODO(fxbug.dev/50542): Update documentation.
  void StopAdvertising(const DeviceAddress& address) override;

  void OnIncomingConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                            const DeviceAddress& peer_address,
                            const hci_spec::LEConnectionParameters& conn_params) override;

 private:
  std::unique_ptr<CommandPacket> BuildEnablePacket(const DeviceAddress& address,
                                                   hci_spec::GenericEnableParam enable) override;

  std::unique_ptr<CommandPacket> BuildSetAdvertisingParams(
      const DeviceAddress& address, hci_spec::LEAdvertisingType type,
      hci_spec::LEOwnAddressType own_address_type, AdvertisingIntervalRange interval) override;

  std::unique_ptr<CommandPacket> BuildSetAdvertisingData(const DeviceAddress& address,
                                                         const AdvertisingData& data,
                                                         AdvFlags flags) override;

  std::unique_ptr<CommandPacket> BuildUnsetAdvertisingData(const DeviceAddress& address) override;

  std::unique_ptr<CommandPacket> BuildSetScanResponse(const DeviceAddress& address,
                                                      const AdvertisingData& scan_rsp) override;

  std::unique_ptr<CommandPacket> BuildUnsetScanResponse(const DeviceAddress& address) override;

  std::unique_ptr<CommandPacket> BuildRemoveAdvertisingSet(const DeviceAddress& address) override;

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

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_ADVERTISER_H_
