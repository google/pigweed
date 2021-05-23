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
  explicit LegacyLowEnergyAdvertiser(fxl::WeakPtr<Transport> hci);
  ~LegacyLowEnergyAdvertiser() override;

  // LowEnergyAdvertiser overrides:
  size_t GetSizeLimit() override { return kMaxLEAdvertisingDataLength; }
  size_t GetMaxAdvertisements() const override { return 1; }
  bool AllowsRandomAddressChange() const override { return !starting_ && !advertising(); }

  // LegacyLowEnergyAdvertiser supports only a single advertising instance,
  // hence it can report additional errors in the following conditions:
  // 1. If called while a start request is pending, reports kRepeatedAttempts.
  // 2. If called while a stop request is pending, then cancels the stop request
  //    and proceeds with start.
  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions adv_options,
                        ConnectionCallback connect_callback,
                        StatusCallback status_callback) override;

  // If called while a stop request is pending, returns false.
  // If called while a start request is pending, then cancels the start
  // request and proceeds with start.
  // Returns false if called while not advertising.
  // TODO(fxbug.dev/50542): Update documentation.
  bool StopAdvertising(const DeviceAddress& address) override;

  // Clears the advertising state before passing |link| on to
  // |connect_callback_|.
  void OnIncomingConnection(ConnectionHandle handle, Connection::Role role,
                            const DeviceAddress& peer_address,
                            const LEConnectionParameters& conn_params) override;

 private:
  // Unconditionally stops advertising.
  void StopAdvertisingInternal();

  void StartAdvertisingInternal(const DeviceAddress& address, const AdvertisingData& data,
                                const AdvertisingData& scan_rsp, AdvertisingIntervalRange interval,
                                AdvFlags flags, ConnectionCallback connect_callback,
                                StatusCallback status_callback);

  // Returns true if currently advertising.
  bool advertising() const { return advertised_ != DeviceAddress(); }

  // The transport that's used to issue commands
  fxl::WeakPtr<Transport> hci_;

  // |hci_cmd_runner_| will be running when a start or stop is pending.
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
  std::unique_ptr<SequentialCommandRunner> hci_cmd_runner_;

  // Non-zero if advertising has been enabled.
  DeviceAddress advertised_;

  // if not null, the callback for connectable advertising.
  ConnectionCallback connect_callback_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyAdvertiser);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_ADVERTISER_H_
