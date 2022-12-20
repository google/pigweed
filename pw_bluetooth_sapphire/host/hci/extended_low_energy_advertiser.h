// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_EXTENDED_LOW_ENERGY_ADVERTISER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_EXTENDED_LOW_ENERGY_ADVERTISER_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/advertising_handle_map.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

class SequentialCommandRunner;

class ExtendedLowEnergyAdvertiser final : public LowEnergyAdvertiser {
 public:
  explicit ExtendedLowEnergyAdvertiser(hci::Transport::WeakPtr hci);
  ~ExtendedLowEnergyAdvertiser() override;

  bool AllowsRandomAddressChange() const override { return !IsAdvertising(); }

  // TODO(fxbug.dev/81470): The maximum length of data that can be advertised. For backwards
  // compatibility and because supporting it is a much larger project, we currently only support
  // legacy PDUs. When using legacy PDUs, the maximum advertising data size is
  // hci_spec::kMaxLEAdvertisingDataLength.
  //
  // TODO(fxbug.dev/77476): Extended advertising supports sending larger amounts of data, but they
  // have to be fragmented across multiple commands to the controller. This is not yet supported in
  // this implementation. We should support larger than kMaxLEExtendedAdvertisingDataLength
  // advertising data with fragmentation.
  size_t GetSizeLimit() const override { return hci_spec::kMaxLEAdvertisingDataLength; }

  // Attempt to start advertising. See LowEnergyAdvertiser::StartAdvertising for full documentation.
  //
  // According to the Bluetooth Spec, Volume 4, Part E, Section 7.8.58, "the number of advertising
  // sets that can be supported is not fixed and the Controller can change it at any time. The
  // memory used to store advertising sets can also be used for other purposes."
  //
  // This method may report an error if the controller cannot currently support another advertising
  // set.
  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions adv_options,
                        ConnectionCallback connect_callback,
                        ResultFunction<> result_callback) override;

  void StopAdvertising() override;
  void StopAdvertising(const DeviceAddress& address) override;

  void OnIncomingConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
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
    hci_spec::ConnectionRole role;
    DeviceAddress peer_address;
    hci_spec::LEConnectionParameters conn_params;
  };

  struct StagedAdvertisingParameters {
    bool include_tx_power_level = false;
    int8_t selected_tx_power_level = 0;
  };

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
                                                      const AdvertisingData& data) override;

  std::unique_ptr<CommandPacket> BuildUnsetScanResponse(const DeviceAddress& address) override;

  std::unique_ptr<CommandPacket> BuildRemoveAdvertisingSet(const DeviceAddress& address) override;

  void OnSetAdvertisingParamsComplete(const EventPacket& event) override;

  void OnCurrentOperationComplete() override;

  // Event handler for the HCI LE Advertising Set Terminated event
  CommandChannel::EventCallbackResult OnAdvertisingSetTerminatedEvent(const EventPacket& event);
  CommandChannel::EventHandlerId set_terminated_event_handler_id_;

  AdvertisingHandleMap advertising_handle_map_;
  std::queue<fit::closure> op_queue_;
  StagedAdvertisingParameters staged_advertising_parameters_;

  // Core Spec Volume 4, Part E, Section 7.8.56: Incoming connections to LE Extended Advertising
  // occur through two events: HCI_LE_Connection_Complete and HCI_LE_Advertising_Set_Terminated.
  // The HCI_LE_Connection_Complete event provides the connection handle along with some other
  // connection related parameters. Notably missing is the advertising handle, which we need to
  // obtain the advertised device address. Until we receive the HCI_LE_Advertising_Set_Terminated
  // event, we stage these parameters.
  std::unordered_map<hci_spec::ConnectionHandle, StagedConnectionParameters>
      staged_connections_map_;

  // Keep this as the last member to make sure that all weak pointers are invalidated before other
  // members get destroyed
  fxl::WeakPtrFactory<ExtendedLowEnergyAdvertiser> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ExtendedLowEnergyAdvertiser);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_EXTENDED_LOW_ENERGY_ADVERTISER_H_
