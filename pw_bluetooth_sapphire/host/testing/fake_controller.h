// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_

#include <lib/async/default.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_class.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_base.h"
#include "src/lib/fxl/functional/cancelable_callback.h"

namespace bt {
namespace testing {

class FakePeer;

// FakeController emulates a real Bluetooth controller. It can be configured to
// respond to HCI commands in a predictable manner.
class FakeController : public FakeControllerBase, public fbl::RefCounted<FakeController> {
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

    // The time elapsed from the receipt of a LE Create Connection command until
    // the resulting LE Connection Complete event.
    zx::duration le_connection_delay;

    // HCI settings.
    hci::HCIVersion hci_version;      // Default: HCIVersion::k5_0.
    uint8_t num_hci_command_packets;  // Default: 1
    uint64_t event_mask;
    uint64_t le_event_mask;

    // BD_ADDR (BR/EDR) or Public Device Address (LE)
    DeviceAddress bd_addr;

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

    BufferView advertised_view() const { return BufferView(data, data_length); }
    BufferView scan_rsp_view() const { return BufferView(scan_rsp_data, scan_rsp_length); }

    bool enabled;
    hci::LEAdvertisingType adv_type;
    hci::LEOwnAddressType own_address_type;
    uint16_t interval_min;
    uint16_t interval_max;

    uint8_t data_length;
    uint8_t data[hci::kMaxLEAdvertisingDataLength];
    uint8_t scan_rsp_length;
    uint8_t scan_rsp_data[hci::kMaxLEAdvertisingDataLength];
  };

  // The parameters of the most recent low energy connection initiation request
  struct LEConnectParams final {
    LEConnectParams() = default;

    hci::LEOwnAddressType own_address_type;
    DeviceAddress peer_address;
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
  const LEAdvertisingState& le_advertising_state() const { return le_adv_state_; }

  // Returns the most recent LE connection request parameters.
  const std::optional<LEConnectParams>& le_connect_params() const { return le_connect_params_; }

  const std::optional<DeviceAddress>& le_random_address() const { return le_random_address_; }

  // Returns the current local name set in the controller
  const std::string& local_name() const { return local_name_; }

  // Returns the current class of device.
  const DeviceClass& device_class() const { return device_class_; }

  // Adds a fake remote peer. Returns false if a peer with the same address was previously
  // added.
  bool AddPeer(std::unique_ptr<FakePeer> peer);

  // Removes a previously registered peer with the given device |address|. Does nothing if |address|
  // is unrecognized.
  void RemovePeer(const DeviceAddress& address);

  // Returns a pointer to the FakePeer with the given |address|. Returns nullptr if the |address|
  // is unknown.
  FakePeer* FindPeer(const DeviceAddress& address);

  // Sets a callback to be invoked when the the base controller parameters change due to a HCI
  // command. These parameters are:
  //
  //   - The local name.
  //   - The local class of device.
  void set_controller_parameters_callback(fit::closure callback) {
    controller_parameters_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked when the scan state changes.
  using ScanStateCallback = fit::function<void(bool enabled)>;
  void set_scan_state_callback(ScanStateCallback callback) { scan_state_cb_ = std::move(callback); }

  // Sets a callback to be invoked when the LE Advertising state changes.
  void set_advertising_state_callback(fit::closure callback) {
    advertising_state_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked on connection events.
  using ConnectionStateCallback = fit::function<void(
      const DeviceAddress&, hci::ConnectionHandle handle, bool connected, bool canceled)>;
  void set_connection_state_callback(ConnectionStateCallback callback) {
    conn_state_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked when LE connection parameters are updated for
  // a fake device.
  using LEConnectionParametersCallback =
      fit::function<void(const DeviceAddress&, const hci::LEConnectionParameters&)>;
  void set_le_connection_parameters_callback(LEConnectionParametersCallback callback) {
    le_conn_params_cb_ = std::move(callback);
  }

  // Sends a HCI event with the given parameters.
  void SendEvent(hci::EventCode event_code, const ByteBuffer& payload);

  // Sends a LE Meta event with the given parameters.
  void SendLEMetaEvent(hci::EventCode subevent_code, const ByteBuffer& payload);

  // Sends an ACL data packet with the given parameters.
  void SendACLPacket(hci::ConnectionHandle handle, const ByteBuffer& payload);

  // Sends a L2CAP basic frame.
  void SendL2CAPBFrame(hci::ConnectionHandle handle, l2cap::ChannelId channel_id,
                       const ByteBuffer& payload);

  // Sends a L2CAP control frame over a signaling channel. If |is_le| is true,
  // then the LE signaling channel will be used.
  void SendL2CAPCFrame(hci::ConnectionHandle handle, bool is_le, l2cap::CommandCode code,
                       uint8_t id, const ByteBuffer& payload);

  void SendNumberOfCompletedPacketsEvent(hci::ConnectionHandle conn, uint16_t num);

  // Sets up a LE link to the device with the given |addr|. FakeController will
  // report a connection event in which it is in the given |role|.
  void ConnectLowEnergy(const DeviceAddress& addr,
                        hci::ConnectionRole role = hci::ConnectionRole::kSlave);

  // Tells a fake device to initiate the L2CAP Connection Parameter Update
  // procedure using the given |params|. Has no effect if a connected fake
  // device with the given |addr| is not found.
  void L2CAPConnectionParameterUpdate(const DeviceAddress& addr,
                                      const hci::LEPreferredConnectionParameters& params);

  // Marks the FakePeer with address |address| as disconnected and sends a HCI
  // Disconnection Complete event for all of its links.
  void Disconnect(const DeviceAddress& addr);

  // Send HCI Disconnection Complete event for |handle|.
  void SendDisconnectionCompleteEvent(hci::ConnectionHandle handle);

  // Callback to invoke when a packet is received over the data channel. Care
  // should be taken to ensure that a callback with a reference to test case
  // variables is not invoked when tearing down.
  using DataCallback = fit::function<void(const ByteBuffer& packet)>;
  void SetDataCallback(DataCallback callback, async_dispatcher_t* dispatcher);
  void ClearDataCallback();

  // Automatically send HCI Number of Completed Packets event for each packet received. Enabled by
  // default.
  void set_auto_completed_packets_event_enabled(bool enabled) {
    auto_completed_packets_event_enabled_ = enabled;
  }

  // Automatically send HCI Disconnection Complete event when HCI Disconnect command received.
  // Enabled by default.
  void set_auto_disconnection_complete_event_enabled(bool enabled) {
    auto_disconnection_complete_event_enabled_ = enabled;
  }

 private:
  // Returns the current thread's task dispatcher.
  async_dispatcher_t* dispatcher() const { return async_get_default_dispatcher(); }

  // Finds and returns the FakePeer with the given parameters or nullptr if no
  // such device exists.
  FakePeer* FindByConnHandle(hci::ConnectionHandle handle);

  // Returns the next available L2CAP signaling channel command ID.
  uint8_t NextL2CAPCommandId();

  // Sends a HCI_Command_Complete event in response to the command with |opcode|
  // and using the given data as the parameter payload.
  void RespondWithCommandComplete(hci::OpCode opcode, const ByteBuffer& params);

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

  // Sends LE advertising reports for all known peers with advertising data, if a
  // scan is currently enabled. If duplicate filtering is disabled then the reports are continued to
  // be sent until scan is disabled.
  void SendAdvertisingReports();

  // Sends a single LE advertising report for the given peer. May send an additional report if the
  // peer has scan response data and was configured to not batch them in a single report alongside
  // the regular advertisement.
  //
  // Does nothing if a LE scan is not currently enabled or if the peer doesn't support advertising.
  void SendSingleAdvertisingReport(const FakePeer& peer);

  // Notifies |controller_parameters_cb_|.
  void NotifyControllerParametersChanged();

  // Notifies |advertising_state_cb_|
  void NotifyAdvertisingState();

  // Notifies |conn_state_cb_| with the given parameters.
  void NotifyConnectionState(const DeviceAddress& addr, hci::ConnectionHandle handle,
                             bool connected, bool canceled = false);

  // Called when a HCI_Create_Connection command is received.
  void OnCreateConnectionCommandReceived(const hci::CreateConnectionCommandParams& params);

  // Notifies |le_conn_params_cb_|
  void NotifyLEConnectionParameters(const DeviceAddress& addr,
                                    const hci::LEConnectionParameters& params);

  // Called when a HCI_LE_Create_Connection command is received.
  void OnLECreateConnectionCommandReceived(const hci::LECreateConnectionCommandParams& params);

  // Called when a HCI_LE_Connection_Update command is received.
  void OnLEConnectionUpdateCommandReceived(const hci::LEConnectionUpdateCommandParams& params);

  // Called when a HCI_Disconnect command is received.
  void OnDisconnectCommandReceived(const hci::DisconnectCommandParams& params);

  // FakeControllerBase overrides:
  void OnCommandPacketReceived(const PacketView<hci::CommandHeader>& command_packet) override;
  void OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) override;

  Settings settings_;
  LEScanState le_scan_state_;
  LEAdvertisingState le_adv_state_;

  // Used for Advertising, Create Connection, and Active Scanning
  // Set by HCI_LE_Set_Random_Address
  std::optional<DeviceAddress> le_random_address_;

  // Used for BR/EDR Scans
  uint8_t bredr_scan_state_;
  hci::PageScanType page_scan_type_;
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;

  // The GAP local name, as written/read by HCI_(Read/Write)_Local_Name. While the aforementioned
  // HCI commands carry the name in a 248 byte buffer, |local_name_| contains the intended value.
  std::string local_name_;

  // The local device class configured by HCI_Write_Class_of_Device.
  DeviceClass device_class_;

  // Variables used for
  // HCI_LE_Create_Connection/HCI_LE_Create_Connection_Cancel.
  uint16_t next_conn_handle_;
  fxl::CancelableClosure pending_le_connect_rsp_;
  std::optional<LEConnectParams> le_connect_params_;
  bool le_connect_pending_;

  // Variables used for
  // HCI_BREDR_Create_Connection/HCI_BREDR_Create_Connection_Cancel.
  bool bredr_connect_pending_;
  DeviceAddress pending_bredr_connect_addr_;
  fxl::CancelableClosure pending_bredr_connect_rsp_;

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

  // The set of fake peers that are visible.
  std::unordered_map<DeviceAddress, std::unique_ptr<FakePeer>> peers_;

  fit::closure controller_parameters_cb_;
  ScanStateCallback scan_state_cb_;
  fit::closure advertising_state_cb_;
  ConnectionStateCallback conn_state_cb_;
  LEConnectionParametersCallback le_conn_params_cb_;

  // Called when ACL data packets received.
  DataCallback data_callback_;
  async_dispatcher_t* data_dispatcher_;

  bool auto_completed_packets_event_enabled_;
  bool auto_disconnection_complete_event_enabled_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeController);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_
