// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ANDROID_EXTENDED_LOW_ENERGY_ADVERTISER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ANDROID_EXTENDED_LOW_ENERGY_ADVERTISER_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/advertising_handle_map.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_advertiser.h"

namespace bt::hci {

class Transport;
class SequentialCommandRunner;

// AndroidExtendedLowEnergyAdvertiser implements chip-based multiple adverting via Android's vendor
// extensions. AndroidExtendedLowEnergyAdvertiser implements a LowEnergyAdvertiser but uses the
// Android vendor HCI extension commands to interface with the controller instead of standard
// Bluetooth Core Specification 5.0+. This enables power efficient multiple advertising for chipsets
// using pre-5.0 versions of Bluetooth.
//
// For more information, see https://source.android.com/devices/bluetooth/hci_requirements
class AndroidExtendedLowEnergyAdvertiser final : public LowEnergyAdvertiser {
 public:
  // Create an AndroidExtendedLowEnergyAdvertiser. The maximum number of advertisements the
  // controller can support (obtained via hci_spec::vendor::android::LEGetVendorCapabilities)
  // should be passed to the constructor via the max_advertisements parameter.
  explicit AndroidExtendedLowEnergyAdvertiser(hci::Transport::WeakPtr hci,
                                              uint8_t max_advertisements);
  ~AndroidExtendedLowEnergyAdvertiser() override;

  // LowEnergyAdvertiser overrides:
  bool AllowsRandomAddressChange() const override { return !IsAdvertising(); }
  size_t GetSizeLimit() const override { return hci_spec::kMaxLEAdvertisingDataLength; }

  // Attempt to start advertising. See LowEnergyAdvertiser::StartAdvertising for full documentation.
  //
  // The number of advertising sets that can be supported is not fixed and the Controller can change
  // it at any time. This method may report an error if the controller cannot currently support
  // another advertising set.
  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions adv_options,
                        ConnectionCallback connect_callback,
                        ResultFunction<> result_callback) override;

  void StopAdvertising() override;
  void StopAdvertising(const DeviceAddress& address) override;

  void OnIncomingConnection(hci_spec::ConnectionHandle handle,
                            pw::bluetooth::emboss::ConnectionRole role,
                            const DeviceAddress& peer_address,
                            const hci_spec::LEConnectionParameters& conn_params) override;

  size_t MaxAdvertisements() const override { return advertising_handle_map_.capacity(); }

  // Returns the last used advertising handle that was used for an advertising set when
  // communicating with the controller.
  std::optional<hci_spec::AdvertisingHandle> LastUsedHandleForTesting() const {
    return advertising_handle_map_.LastUsedHandleForTesting();
  }

 private:
  struct StagedConnectionParameters {
    pw::bluetooth::emboss::ConnectionRole role;
    DeviceAddress peer_address;
    hci_spec::LEConnectionParameters conn_params;
  };

  std::optional<EmbossCommandPacket> BuildEnablePacket(
      const DeviceAddress& address, pw::bluetooth::emboss::GenericEnableParam enable) override;

  CommandChannel::CommandPacketVariant BuildSetAdvertisingParams(
      const DeviceAddress& address, pw::bluetooth::emboss::LEAdvertisingType type,
      pw::bluetooth::emboss::LEOwnAddressType own_address_type,
      AdvertisingIntervalRange interval) override;

  CommandChannel::CommandPacketVariant BuildSetAdvertisingData(const DeviceAddress& address,
                                                               const AdvertisingData& data,
                                                               AdvFlags flags) override;

  CommandChannel::CommandPacketVariant BuildUnsetAdvertisingData(
      const DeviceAddress& address) override;

  CommandChannel::CommandPacketVariant BuildSetScanResponse(
      const DeviceAddress& address, const AdvertisingData& scan_rsp) override;

  CommandChannel::CommandPacketVariant BuildUnsetScanResponse(
      const DeviceAddress& address) override;

  std::optional<EmbossCommandPacket> BuildRemoveAdvertisingSet(
      const DeviceAddress& address) override;

  void OnCurrentOperationComplete() override;

  // Event handler for the LE multi-advertising state change sub-event
  CommandChannel::EventCallbackResult OnAdvertisingStateChangedSubevent(
      const EmbossEventPacket& event);
  CommandChannel::EventHandlerId state_changed_event_handler_id_;

  uint8_t max_advertisements_ = 0;
  AdvertisingHandleMap advertising_handle_map_;
  std::queue<fit::closure> op_queue_;

  // Incoming connections to Android LE Multiple Advertising occur through two events:
  // HCI_LE_Connection_Complete and LE multi-advertising state change subevent. The
  // HCI_LE_Connection_Complete event provides the connection handle along with some other
  // connection related parameters. Notably missing is the advertising handle, which we need to
  // obtain the advertised device address. Until we receive the LE multi-advertising state change
  // subevent, we stage these parameters.
  std::unordered_map<hci_spec::ConnectionHandle, StagedConnectionParameters>
      staged_connections_map_;

  // Keep this as the last member to make sure that all weak pointers are invalidated before other
  // members get destroyed
  WeakSelf<AndroidExtendedLowEnergyAdvertiser> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AndroidExtendedLowEnergyAdvertiser);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ANDROID_EXTENDED_LOW_ENERGY_ADVERTISER_H_
