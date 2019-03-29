// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_base.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/macros.h"

namespace bt {
namespace testing {

class FakeDevice;

// FakeController emulates a real Bluetooth controller. It can be configured to
// respond to HCI commands in a predictable manner.
class FakeController : public FakeControllerBase,
                       public fbl::RefCounted<FakeController> {
 public:
  // Global settings for the FakeController. These can be used to initialize a
  // FakeController and/or to re-configure an existing one.
  struct Settings final {
    // The default constructor initializes all fields to 0, unless another
    // default is specified below.
    Settings();
    ~Settings() = default;

    void ApplyDualModeDefaults();
    void ApplyLEOnlyDefaults();
    void ApplyLegacyLEConfig();
    void ApplyLEConfig();

    void AddBREDRSupportedCommands();
    void AddLESupportedCommands();

    // HCI settings.
    hci::HCIVersion hci_version;      // Default: HCIVersion::k5_0.
    uint8_t num_hci_command_packets;  // Default: 1
    uint64_t event_mask;
    uint64_t le_event_mask;

    // BD_ADDR (BR/EDR) or Public Device Address (LE)
    common::DeviceAddress bd_addr;

    // Local supported features and commands.
    uint64_t lmp_features_page0;
    uint64_t lmp_features_page1;
    uint64_t lmp_features_page2;
    uint64_t le_features;
    uint64_t le_supported_states;
    uint8_t supported_commands[64];

    // Buffer Size.
    uint16_t acl_data_packet_length;
    uint8_t total_num_acl_data_packets;
    uint16_t le_acl_data_packet_length;
    uint8_t le_total_num_acl_data_packets;
  };

  // Current device low energy scan state.
  struct LEScanState final {
    LEScanState();

    bool enabled;
    hci::LEScanType scan_type;
    uint16_t scan_interval;
    uint16_t scan_window;
    bool filter_duplicates;
    hci::LEOwnAddressType own_address_type;
    hci::LEScanFilterPolicy filter_policy;
  };

  // Current device basic advertising state
  struct LEAdvertisingState final {
    LEAdvertisingState();

    common::BufferView advertised_view() const {
      return common::BufferView(data, data_length);
    }
    common::BufferView scan_rsp_view() const {
      return common::BufferView(scan_rsp_data, scan_rsp_length);
    }

    bool enabled;
    hci::LEAdvertisingType adv_type;
    hci::LEOwnAddressType own_address_type;
    uint16_t interval;

    uint8_t data_length;
    uint8_t data[hci::kMaxLEAdvertisingDataLength];
    uint8_t scan_rsp_length;
    uint8_t scan_rsp_data[hci::kMaxLEAdvertisingDataLength];
  };

  // Constructor initializes the controller with the minimal default settings
  // (equivalent to calling Settings::ApplyDefaults()).
  FakeController();
  ~FakeController() override;

  // Resets the controller settings.
  void set_settings(const Settings& settings) { settings_ = settings; }

  // Tells the FakeController to always respond to the given command opcode with
  // the given HCI status code.
  void SetDefaultResponseStatus(hci::OpCode opcode, hci::StatusCode status);
  void ClearDefaultResponseStatus(hci::OpCode opcode);

  // Returns the current LE scan state.
  const LEScanState& le_scan_state() const { return le_scan_state_; }

  // Returns the current LE advertising state.
  const LEAdvertisingState& le_advertising_state() const {
    return le_adv_state_;
  }

  const std::optional<common::DeviceAddress>& le_random_address() const {
    return le_random_address_;
  }

  // Returns the current Local Name.set in the controller
  const std::string& local_name() const { return local_name_; }

  // Adds a fake remote device.
  void AddDevice(std::unique_ptr<FakeDevice> device);

  // Sets a callback to be invoked when the scan state changes.
  using ScanStateCallback = fit::function<void(bool enabled)>;
  void SetScanStateCallback(ScanStateCallback callback,
                            async_dispatcher_t* dispatcher);

  // Sets a callback to be invoked when the LE Advertising state changes.
  void SetAdvertisingStateCallback(fit::closure callback,
                                   async_dispatcher_t* dispatcher);

  // Sets a callback to be invoked on connection events.
  using ConnectionStateCallback = fit::function<void(
      const common::DeviceAddress&, bool connected, bool canceled)>;
  void SetConnectionStateCallback(ConnectionStateCallback callback,
                                  async_dispatcher_t* dispatcher);

  // Sets a callback to be invoked when LE connection parameters are updated for
  // a fake device.
  using LEConnectionParametersCallback = fit::function<void(
      const common::DeviceAddress&, const hci::LEConnectionParameters&)>;
  void SetLEConnectionParametersCallback(
      LEConnectionParametersCallback callback, async_dispatcher_t* dispatcher);

  // Sends a HCI event with the given parameters.
  void SendEvent(hci::EventCode event_code, const common::ByteBuffer& payload);

  // Sends a LE Meta event with the given parameters.
  void SendLEMetaEvent(hci::EventCode subevent_code,
                       const common::ByteBuffer& payload);

  // Sends an ACL data packet with the given parameters.
  void SendACLPacket(hci::ConnectionHandle handle,
                     const common::ByteBuffer& payload);

  // Sends a L2CAP basic frame.
  void SendL2CAPBFrame(hci::ConnectionHandle handle,
                       l2cap::ChannelId channel_id,
                       const common::ByteBuffer& payload);

  // Sends a L2CAP control frame over a signaling channel. If |is_le| is true,
  // then the LE signaling channel will be used.
  void SendL2CAPCFrame(hci::ConnectionHandle handle, bool is_le,
                       l2cap::CommandCode code, uint8_t id,
                       const common::ByteBuffer& payload);

  void SendNumberOfCompletedPacketsEvent(hci::ConnectionHandle conn,
                                         uint16_t num);

  // Sets up a LE link to the device with the given |addr|. FakeController will
  // report a connection event in which it is in the given |role|.
  void ConnectLowEnergy(const common::DeviceAddress& addr,
                        hci::ConnectionRole role = hci::ConnectionRole::kSlave);

  // Tells a fake device to initiate the L2CAP Connection Parameter Update
  // procedure using the given |params|. Has no effect if a connected fake
  // device with the given |addr| is not found.
  void L2CAPConnectionParameterUpdate(
      const common::DeviceAddress& addr,
      const hci::LEPreferredConnectionParameters& params);

  // Marks the FakeDevice with address |address| as disconnected and sends a HCI
  // Disconnection Complete event for all of its links.
  void Disconnect(const common::DeviceAddress& addr);

 private:
  // Returns the current thread's task dispatcher.
  async_dispatcher_t* dispatcher() const {
    return async_get_default_dispatcher();
  }

  // Finds and returns the FakeDevice with the given parameters or nullptr if no
  // such device exists.
  FakeDevice* FindDeviceByAddress(const common::DeviceAddress& addr);
  FakeDevice* FindDeviceByConnHandle(hci::ConnectionHandle handle);

  // Returns the next available L2CAP signaling channel command ID.
  uint8_t NextL2CAPCommandId();

  // Sends a HCI_Command_Complete event in response to the command with |opcode|
  // and using the given data as the parameter payload.
  void RespondWithCommandComplete(hci::OpCode opcode,
                                  const common::ByteBuffer& params);

  // Sends a HCI_Command_Complete event with "Success" status in response to the
  // command with |opcode|.
  void RespondWithSuccess(hci::OpCode opcode);

  // Sends a HCI_Command_Status event in response to the command with |opcode|
  // and using the given data as the parameter payload.
  void RespondWithCommandStatus(hci::OpCode opcode, hci::StatusCode status);

  // If a default status has been configured for the given opcode, sends back an
  // error response and returns true. Returns false if no response was set.
  bool MaybeRespondWithDefaultStatus(hci::OpCode opcode);

  // Sends Inquiry Response reports for known BR/EDR devices.
  void SendInquiryResponses();

  // Sends LE advertising reports for known devices with advertising data, if a
  // scan is currently enabled.
  void SendAdvertisingReports();

  // Notifies |advertising_state_cb_|
  void NotifyAdvertisingState();

  // Notifies |conn_state_cb_| with the given parameters.
  void NotifyConnectionState(const common::DeviceAddress& addr, bool connected,
                             bool canceled = false);

  // Notifies |le_conn_params_cb_|
  void NotifyLEConnectionParameters(const common::DeviceAddress& addr,
                                    const hci::LEConnectionParameters& params);

  // Called when a HCI_LE_Create_Connection command is received.
  void OnLECreateConnectionCommandReceived(
      const hci::LECreateConnectionCommandParams& params);

  // Called when a HCI_LE_Connection_Update command is received.
  void OnLEConnectionUpdateCommandReceived(
      const hci::LEConnectionUpdateCommandParams& params);

  // Called when a HCI_Disconnect command is received.
  void OnDisconnectCommandReceived(const hci::DisconnectCommandParams& params);

  // FakeControllerBase overrides:
  void OnCommandPacketReceived(
      const common::PacketView<hci::CommandHeader>& command_packet) override;
  void OnACLDataPacketReceived(
      const common::ByteBuffer& acl_data_packet) override;

  Settings settings_;
  LEScanState le_scan_state_;
  LEAdvertisingState le_adv_state_;

  // Used for Advertising, Create Connection, and Active Scanning
  // Set by HCI_LE_Set_Random_Address
  std::optional<common::DeviceAddress> le_random_address_;

  // Used for BR/EDR Scans
  uint8_t bredr_scan_state_;
  hci::PageScanType page_scan_type_;
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;

  // The GAP local name, as written/read by HCI_(Read/Write)_Local_Name
  std::string local_name_;

  // Variables used for
  // HCI_LE_Create_Connection/HCI_LE_Create_Connection_Cancel.
  uint16_t next_conn_handle_;
  fxl::CancelableClosure pending_le_connect_rsp_;
  common::DeviceAddress pending_le_connect_addr_;
  bool le_connect_pending_;

  // ID used for L2CAP LE signaling channel commands.
  uint8_t next_le_sig_id_;

  // The Inquiry Mode that the controller is in.  Determines what types of
  // events are faked when a kInquiry is started.
  hci::InquiryMode inquiry_mode_;

  // The number of results left in Inquiry Mode operation.
  // If negative, no limit has been set.
  int16_t inquiry_num_responses_left_;

  // Used to setup default status responses (for simulating errors)
  std::unordered_map<hci::OpCode, hci::StatusCode> default_status_map_;

  // The set of fake devices that are visible.
  std::vector<std::unique_ptr<FakeDevice>> devices_;

  ScanStateCallback scan_state_cb_;
  async_dispatcher_t* scan_state_cb_dispatcher_;

  fit::closure advertising_state_cb_;
  async_dispatcher_t* advertising_state_cb_dispatcher_;

  ConnectionStateCallback conn_state_cb_;
  async_dispatcher_t* conn_state_cb_dispatcher_;

  LEConnectionParametersCallback le_conn_params_cb_;
  async_dispatcher_t* le_conn_params_cb_dispatcher_;

  FXL_DISALLOW_COPY_AND_ASSIGN(FakeController);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_
