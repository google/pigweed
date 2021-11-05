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
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/le_connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test_double_base.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/lib/fxl/functional/cancelable_callback.h"

namespace bt::testing {

class FakePeer;

// FakeController emulates a real Bluetooth controller. It can be configured to respond to HCI
// commands in a predictable manner.
class FakeController : public ControllerTestDoubleBase, public fbl::RefCounted<FakeController> {
 public:
  // Global settings for the FakeController. These can be used to initialize a FakeController and/or
  // to re-configure an existing one.
  struct Settings final {
    // The default constructor initializes all fields to 0, unless another default is specified
    // below.
    Settings();
    ~Settings() = default;

    void ApplyDualModeDefaults();
    void ApplyLEOnlyDefaults();
    void ApplyLegacyLEConfig();
    void ApplyLEConfig();

    void AddBREDRSupportedCommands();
    void AddLESupportedCommands();

    // The time elapsed from the receipt of a LE Create Connection command until the resulting LE
    // Connection Complete event.
    zx::duration le_connection_delay;

    // HCI settings.
    hci_spec::HCIVersion hci_version;  // Default: HCIVersion::k5_0.
    uint8_t num_hci_command_packets;   // Default: 1
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
    bool enabled = false;
    hci_spec::LEScanType scan_type = hci_spec::LEScanType::kPassive;
    uint16_t scan_interval = 0;
    uint16_t scan_window = 0;
    bool filter_duplicates = false;
    hci_spec::LEOwnAddressType own_address_type = hci_spec::LEOwnAddressType::kPublic;
    hci_spec::LEScanFilterPolicy filter_policy = hci_spec::LEScanFilterPolicy::kNoFilterAcceptList;
  };

  // Current device basic advertising state
  struct LEAdvertisingState final {
    BufferView advertised_view() const { return BufferView(data, data_length); }
    BufferView scan_rsp_view() const { return BufferView(scan_rsp_data, scan_rsp_length); }

    bool IsDirectedAdvertising() const;
    bool IsScannableAdvertising() const;
    bool IsConnectableAdvertising() const;

    bool enabled = false;
    hci_spec::LEAdvertisingType adv_type = hci_spec::LEAdvertisingType::kAdvInd;

    std::optional<DeviceAddress> random_address;
    hci_spec::LEOwnAddressType own_address_type = hci_spec::LEOwnAddressType::kPublic;

    uint32_t interval_min = 0;
    uint32_t interval_max = 0;

    uint8_t data_length = 0;
    uint8_t data[hci_spec::kMaxLEAdvertisingDataLength] = {0};
    uint8_t scan_rsp_length = 0;
    uint8_t scan_rsp_data[hci_spec::kMaxLEAdvertisingDataLength] = {0};
  };

  // The parameters of the most recent low energy connection initiation request
  struct LEConnectParams final {
    LEConnectParams() = default;

    hci_spec::LEOwnAddressType own_address_type;
    DeviceAddress peer_address;
  };

  // Constructor initializes the controller with the minimal default settings (equivalent to calling
  // Settings::ApplyDefaults()).
  FakeController() : weak_ptr_factory_(this) {}
  ~FakeController() override { Stop(); }

  // Resets the controller settings.
  void set_settings(const Settings& settings) { settings_ = settings; }

  // Always respond to the given command |opcode| with an Command Status event specifying |status|.
  void SetDefaultCommandStatus(hci_spec::OpCode opcode, hci_spec::StatusCode status);
  void ClearDefaultCommandStatus(hci_spec::OpCode opcode);

  // Tells the FakeController to always respond to the given command opcode with a Command Complete
  // event specifying the given HCI status code.
  void SetDefaultResponseStatus(hci_spec::OpCode opcode, hci_spec::StatusCode status);
  void ClearDefaultResponseStatus(hci_spec::OpCode opcode);

  // Returns the current LE scan state.
  const LEScanState& le_scan_state() const { return le_scan_state_; }

  // Returns the current LE advertising state for legacy advertising
  const LEAdvertisingState& legacy_advertising_state() const { return legacy_advertising_state_; }

  // Returns the current LE advertising state for extended advertising, for the given advertising
  // handle
  const LEAdvertisingState& extended_advertising_state(hci_spec::AdvertisingHandle handle) {
    return extended_advertising_states_[handle];
  }

  // Returns the most recent LE connection request parameters.
  const std::optional<LEConnectParams>& le_connect_params() const { return le_connect_params_; }

  // Returns the current local name set in the controller
  const std::string& local_name() const { return local_name_; }

  // Returns the current class of device.
  const DeviceClass& device_class() const { return device_class_; }

  // Adds a fake remote peer. Returns false if a peer with the same address was previously added.
  bool AddPeer(std::unique_ptr<FakePeer> peer);

  // Removes a previously registered peer with the given device |address|. Does nothing if |address|
  // is unrecognized.
  void RemovePeer(const DeviceAddress& address);

  // Returns a pointer to the FakePeer with the given |address|. Returns nullptr if the |address|
  // is unknown.
  FakePeer* FindPeer(const DeviceAddress& address);

  // Counters for HCI commands received.
  int le_create_connection_command_count() const { return le_create_connection_command_count_; }
  int acl_create_connection_command_count() const { return acl_create_connection_command_count_; }

  // Setting this callback allows test code to introspect the LECreateConnectionCommandParams
  // passed into the LECreateConnectionCommand by bt-host, but does not affect FakeController's
  // handling of the command (i.e. this method exists solely for introspection). To change how
  // FakeController responds to an LECreateConnectionCommand, use the FakePeer::set_connect_status
  // or FakePeer::set_connect_response methods.
  void set_le_create_connection_command_callback(
      fit::function<void(hci_spec::LECreateConnectionCommandParams)> callback) {
    le_create_connection_cb_ = std::move(callback);
  }

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
      const DeviceAddress&, hci_spec::ConnectionHandle handle, bool connected, bool canceled)>;
  void set_connection_state_callback(ConnectionStateCallback callback) {
    conn_state_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked when LE connection parameters are updated for
  // a fake device.
  using LEConnectionParametersCallback =
      fit::function<void(const DeviceAddress&, const hci_spec::LEConnectionParameters&)>;
  void set_le_connection_parameters_callback(LEConnectionParametersCallback callback) {
    le_conn_params_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked just before LE Read Remote Feature commands are handled.
  void set_le_read_remote_features_callback(fit::closure callback) {
    le_read_remote_features_cb_ = std::move(callback);
  }

  // Sets a callback to be invoked when a vendor command is received. A command complete event with
  // the status code returned by the callback will be sent in response. If no callback is set, the
  // kUnknownCommand status will be returned.
  using VendorCommandCallback =
      fit::function<hci_spec::StatusCode(const PacketView<hci_spec::CommandHeader>&)>;
  void set_vendor_command_callback(VendorCommandCallback callback) {
    vendor_command_cb_ = std::move(callback);
  }

  // Sends a HCI event with the given parameters.
  void SendEvent(hci_spec::EventCode event_code, const ByteBuffer& payload);

  // Sends a LE Meta event with the given parameters.
  void SendLEMetaEvent(hci_spec::EventCode subevent_code, const ByteBuffer& payload);

  // Sends an ACL data packet with the given parameters.
  void SendACLPacket(hci_spec::ConnectionHandle handle, const ByteBuffer& payload);

  // Sends a L2CAP basic frame.
  void SendL2CAPBFrame(hci_spec::ConnectionHandle handle, l2cap::ChannelId channel_id,
                       const ByteBuffer& payload);

  // Sends a L2CAP control frame over a signaling channel. If |is_le| is true,
  // then the LE signaling channel will be used.
  void SendL2CAPCFrame(hci_spec::ConnectionHandle handle, bool is_le, l2cap::CommandCode code,
                       uint8_t id, const ByteBuffer& payload);

  void SendNumberOfCompletedPacketsEvent(hci_spec::ConnectionHandle conn, uint16_t num);

  // Sets up a LE link to the device with the given |addr|. FakeController will
  // report a connection event in which it is in the given |role|.
  void ConnectLowEnergy(const DeviceAddress& addr,
                        hci_spec::ConnectionRole role = hci_spec::ConnectionRole::kPeripheral);

  // Sends an HCI Connection Request event.
  void SendConnectionRequest(const DeviceAddress& addr, hci_spec::LinkType link_type);

  // Tells a fake device to initiate the L2CAP Connection Parameter Update
  // procedure using the given |params|. Has no effect if a connected fake
  // device with the given |addr| is not found.
  void L2CAPConnectionParameterUpdate(const DeviceAddress& addr,
                                      const hci_spec::LEPreferredConnectionParameters& params);

  // Sends an LE Meta Event Connection Update Complete Subevent. Used to simulate updates initiated
  // by LE central or spontaneously by the controller.
  void SendLEConnectionUpdateCompleteSubevent(
      hci_spec::ConnectionHandle handle, const hci_spec::LEConnectionParameters& params,
      hci_spec::StatusCode status = hci_spec::StatusCode::kSuccess);

  // Marks the FakePeer with address |address| as disconnected and sends a HCI
  // Disconnection Complete event for all of its links.
  void Disconnect(const DeviceAddress& addr,
                  hci_spec::StatusCode reason = hci_spec::kRemoteUserTerminatedConnection);

  // Send HCI Disconnection Complete event for |handle|.
  void SendDisconnectionCompleteEvent(
      hci_spec::ConnectionHandle handle,
      hci_spec::StatusCode reason = hci_spec::kRemoteUserTerminatedConnection);

  // Send HCI encryption change event for |handle| with the given parameters.
  void SendEncryptionChangeEvent(hci_spec::ConnectionHandle handle, hci_spec::StatusCode status,
                                 hci_spec::EncryptionStatus encryption_enabled);

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

  // Sets the response flag for a TX Power Level Read.
  // Enabled by default (i.e it will respond to TXPowerLevelRead by default).
  void set_tx_power_level_read_response_flag(bool respond) { respond_to_tx_power_read_ = respond; }

  // Upon reception of a command packet with `opcode`, FakeController invokes `pause_listener` with
  // a closure. The command will hang until this closure is invoked, enabling clients to control the
  // timing of command completion.
  void pause_responses_for_opcode(hci_spec::OpCode code,
                                  fit::function<void(fit::closure)> pause_listener) {
    paused_opcode_listeners_[code] = std::move(pause_listener);
  }

  void clear_pause_listener_for_opcode(hci_spec::OpCode code) {
    paused_opcode_listeners_.erase(code);
  }

  // Called when a HCI_LE_Read_Advertising_Channel_Tx_Power command is received.
  void OnLEReadAdvertisingChannelTxPower();

  // Inform the controller that the advertising handle is connected via the connection handle. This
  // method then generates the necessary LE Meta Events (e.g. Advertising Set Terminated) to inform
  // extended advertising listeners.
  void SendLEAdvertisingSetTerminatedEvent(hci_spec::ConnectionHandle conn_handle,
                                           hci_spec::AdvertisingHandle adv_handle);

  // The maximum number of advertising sets supported by the controller. Core Spec Volume 4, Part E,
  // Section 7.8.58: the memory used to store advertising sets can also be used for other purposes.
  // This value can change over time.
  uint8_t num_supported_advertising_sets() const { return num_supported_advertising_sets_; }
  void set_num_supported_advertising_sets(uint8_t value) {
    ZX_ASSERT(value >= extended_advertising_states_.size());
    ZX_ASSERT(value <= hci_spec::kAdvertisingHandleMax + 1);  // support advertising handle of 0
    num_supported_advertising_sets_ = value;
  }

 private:
  // Convert an extended advertising interval to a legacy advertising interval
  static uint32_t DecodeExtendedAdvertisingInterval(const uint8_t (&input)[3]);

  static bool IsValidAdvertisingHandle(hci_spec::AdvertisingHandle handle) {
    return handle <= hci_spec::kAdvertisingHandleMax;
  }

  // Returns the current thread's task dispatcher.
  async_dispatcher_t* dispatcher() const { return async_get_default_dispatcher(); }

  // Finds and returns the FakePeer with the given parameters or nullptr if no
  // such device exists.
  FakePeer* FindByConnHandle(hci_spec::ConnectionHandle handle);

  // Returns the next available L2CAP signaling channel command ID.
  uint8_t NextL2CAPCommandId();

  // Sends a HCI_Command_Complete event with the given status in response to the command with
  // |opcode|.
  //
  // NOTE: This method returns only a status field. Some HCI commands have multiple fields in their
  // return message. In those cases, it's better (and clearer) to use the other
  // RespondWithCommandComplete (ByteBuffer as second parameter) instead.
  void RespondWithCommandComplete(hci_spec::OpCode opcode, hci_spec::StatusCode status);

  // Sends a HCI_Command_Complete event in response to the command with |opcode| and using the given
  // data as the parameter payload.
  void RespondWithCommandComplete(hci_spec::OpCode opcode, const ByteBuffer& params);

  // Sends a HCI_Command_Status event in response to the command with |opcode| and using the given
  // data as the parameter payload.
  void RespondWithCommandStatus(hci_spec::OpCode opcode, hci_spec::StatusCode status);

  // If a default Command Status event status has been set for the given |opcode|, send a Command
  // Status event and returns true.
  bool MaybeRespondWithDefaultCommandStatus(hci_spec::OpCode opcode);

  // If a default status has been configured for the given opcode, sends back an error Command
  // Complete event and returns true. Returns false if no response was set.
  bool MaybeRespondWithDefaultStatus(hci_spec::OpCode opcode);

  // Sends Inquiry Response reports for known BR/EDR devices.
  void SendInquiryResponses();

  // Sends LE advertising reports for all known peers with advertising data, if a scan is currently
  // enabled. If duplicate filtering is disabled then the reports are continued to be sent until
  // scan is disabled.
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
  void NotifyConnectionState(const DeviceAddress& addr, hci_spec::ConnectionHandle handle,
                             bool connected, bool canceled = false);

  // Called when a HCI_Create_Connection command is received.
  void OnCreateConnectionCommandReceived(const hci_spec::CreateConnectionCommandParams& params);

  // Notifies |le_conn_params_cb_|
  void NotifyLEConnectionParameters(const DeviceAddress& addr,
                                    const hci_spec::LEConnectionParameters& params);

  // Called when a HCI_LE_Create_Connection command is received.
  void OnLECreateConnectionCommandReceived(const hci_spec::LECreateConnectionCommandParams& params);

  // Called when a HCI_LE_Connection_Update command is received.
  void OnLEConnectionUpdateCommandReceived(const hci_spec::LEConnectionUpdateCommandParams& params);

  // Called when a HCI_Disconnect command is received.
  void OnDisconnectCommandReceived(const hci_spec::DisconnectCommandParams& params);

  // Called when a HCI_LE_Write_Host_Support command is received.
  void OnWriteLEHostSupportCommandReceived(const hci_spec::WriteLEHostSupportCommandParams& params);

  // Called when a HCI_Reset command is received.
  void OnReset();

  // Called when a HCI_Inquiry command is received.
  void OnInquiry(const hci_spec::InquiryCommandParams& params);

  // Called when a HCI_LE_Set_Scan_Enable command is received.
  void OnLESetScanEnable(const hci_spec::LESetScanEnableCommandParams& params);

  // Called when a HCI_LE_Set_Scan_Parameters command is received.
  void OnLESetScanParamaters(const hci_spec::LESetScanParametersCommandParams& params);

  // Called when a HCI_Read_Local_Extended_Features command is received.
  void OnReadLocalExtendedFeatures(const hci_spec::ReadLocalExtendedFeaturesCommandParams& params);

  // Called when a HCI_SetEventMask command is received.
  void OnSetEventMask(const hci_spec::SetEventMaskCommandParams& params);

  // Called when a HCI_LE_Set_Event_Mask command is received.
  void OnLESetEventMask(const hci_spec::LESetEventMaskCommandParams& params);

  // Called when a HCI_LE_Read_Buffer_Size command is received.
  void OnLEReadBufferSize();

  // Called when a HCI_LE_Read_Supported_States command is received.
  void OnLEReadSupportedStates();

  // Called when a HCI_LE_Read_Local_Supported_Features command is received.
  void OnLEReadLocalSupportedFeatures();

  // Called when a HCI_LE_Create_Connection_Cancel command is received.
  void OnLECreateConnectionCancel();

  // Called when a HCI_Write_Extended_Inquiry_Response command is received.
  void OnWriteExtendedInquiryResponse(const hci_spec::WriteExtendedInquiryResponseParams& params);

  // Called when a HCI_Write_Simple_PairingMode command is received.
  void OnWriteSimplePairingMode(const hci_spec::WriteSimplePairingModeCommandParams& params);

  // Called when a HCI_Read_Simple_Pairing_Mode command is received.
  void OnReadSimplePairingMode();

  // Called when a HCI_Write_Page_Scan_Type command is received.
  void OnWritePageScanType(const hci_spec::WritePageScanTypeCommandParams& params);

  // Called when a HCI_Read_Page_Scan_Type command is received.
  void OnReadPageScanType();

  // Called when a HCI_Write_Inquiry_Mode command is received.
  void OnWriteInquiryMode(const hci_spec::WriteInquiryModeCommandParams& params);

  // Called when a HCI_Read_Inquiry_Mode command is received.
  void OnReadInquiryMode();

  // Called when a HCI_Write_Class_OfDevice command is received.
  void OnWriteClassOfDevice(const hci_spec::WriteClassOfDeviceCommandParams& params);

  // Called when a HCI_Write_Page_Scan_Activity command is received.
  void OnWritePageScanActivity(const hci_spec::WritePageScanActivityCommandParams& params);

  // Called when a HCI_Read_Page_Scan_Activity command is received.
  void OnReadPageScanActivity();

  // Called when a HCI_Write_Scan_Enable command is received.
  void OnWriteScanEnable(const hci_spec::WriteScanEnableCommandParams& params);

  // Called when a HCI_Read_Scan_Enable command is received.
  void OnReadScanEnable();

  // Called when a HCI_Read_Local_Name command is received.
  void OnReadLocalName();

  // Called when a HCI_Write_Local_Name command is received.
  void OnWriteLocalName(const hci_spec::WriteLocalNameCommandParams& params);

  // Called when a HCI_Create_Connection_Cancel command is received.
  void OnCreateConnectionCancel();

  // Called when a HCI_Read_Buffer_Size command is received.
  void OnReadBufferSize();

  // Called when a HCI_Read_BRADDR command is received.
  void OnReadBRADDR();

  // Called when a HCI_LE_Set_Advertising_Enable command is received.
  void OnLESetAdvertisingEnable(const hci_spec::LESetAdvertisingEnableCommandParams& params);

  // Called when a HCI_LE_Set_Scan_Response_Data command is received.
  void OnLESetScanResponseData(const hci_spec::LESetScanResponseDataCommandParams& params);

  // Called when a HCI_LE_Set_Advertising_Data command is received.
  void OnLESetAdvertisingData(const hci_spec::LESetAdvertisingDataCommandParams& params);

  // Called when a HCI_LE_Set_Advertising_Parameters command is received.
  void OnLESetAdvertisingParameters(
      const hci_spec::LESetAdvertisingParametersCommandParams& params);

  // Called when a HCI_LE_Set_Random_Address command is received.
  void OnLESetRandomAddress(const hci_spec::LESetRandomAddressCommandParams& params);

  // Called when a HCI_LE_Set_Advertising_Set_Random_Address command is received.
  void OnLESetAdvertisingSetRandomAddress(
      const hci_spec::LESetAdvertisingSetRandomAddressCommandParams& params);

  // Called when a HCI_LE_Set_Extended_Advertising_Data command is received.
  void OnLESetExtendedAdvertisingParameters(
      const hci_spec::LESetExtendedAdvertisingParametersCommandParams& params);

  // Called when a HCI_LE_Set_Extended_Advertising_Data command is received.
  void OnLESetExtendedAdvertisingData(
      const hci_spec::LESetExtendedAdvertisingDataCommandParams& params);

  // Called when a HCI_LE_Set_Extended_Scan_Response_Data command is received.
  void OnLESetExtendedScanResponseData(
      const hci_spec::LESetExtendedScanResponseDataCommandParams& params);

  // Called when a HCI_LE_Set_Extended_Advertising_Enable command is received.
  void OnLESetExtendedAdvertisingEnable(
      const hci_spec::LESetExtendedAdvertisingEnableCommandParams& params);

  // Called when a HCI_LE_Read_Maximum_Advertising_Data_Length command is received.
  void OnLEReadMaximumAdvertisingDataLength();

  // Called when a HCI_LE_Read_Number_of_Supported_Advertising_Sets command is received.
  void OnLEReadNumberOfSupportedAdvertisingSets();

  // Called when a HCI_LE_Remove_Advertising_Set command is received.
  void OnLERemoveAdvertisingSet(const hci_spec::LERemoveAdvertisingSetCommandParams& params);

  // Called when a HCI_LE_Clear_Advertising_Sets command is received.
  void OnLEClearAdvertisingSets();

  // Called when a HCI_Read_Local_Supported_Features command is received.
  void OnReadLocalSupportedFeatures();

  // Called when a HCI_Read_Local_Supported_Commands command is received.
  void OnReadLocalSupportedCommands();

  // Called when a HCI_Read_Local_Version_Info command is received.
  void OnReadLocalVersionInfo();

  // Interrogation command handlers:

  // Called when a HCI_Read_Remote_Name_Request command is received.
  void OnReadRemoteNameRequestCommandReceived(
      const hci_spec::RemoteNameRequestCommandParams& params);

  // Called when a HCI_Read_Remote_Supported_Features command is received.
  void OnReadRemoteSupportedFeaturesCommandReceived(
      const hci_spec::ReadRemoteSupportedFeaturesCommandParams& params);

  // Called when a HCI_Read_Remote_Version_Information command is received.
  void OnReadRemoteVersionInfoCommandReceived(
      const hci_spec::ReadRemoteVersionInfoCommandParams& params);

  // Called when a HCI_Read_Remote_Extended_Features command is received.
  void OnReadRemoteExtendedFeaturesCommandReceived(
      const hci_spec::ReadRemoteExtendedFeaturesCommandParams& params);

  // Pairing command handlers:

  // Called when a HCI_Authentication_Requested command is received.
  void OnAuthenticationRequestedCommandReceived(
      const hci_spec::AuthenticationRequestedCommandParams& params);

  // Called when a HCI_Link_Key_Request_Reply command is received.
  void OnLinkKeyRequestReplyCommandReceived(
      const hci_spec::LinkKeyRequestReplyCommandParams& params);

  // Called when a HCI_Link_Key_Request_Negative_Reply command is received.
  void OnLinkKeyRequestNegativeReplyCommandReceived(
      const hci_spec::LinkKeyRequestNegativeReplyCommandParams& params);

  // Called when a HCI_IO_Capability_Request_Reply command is received.
  void OnIOCapabilityRequestReplyCommand(
      const hci_spec::IOCapabilityRequestReplyCommandParams& params);

  // Called when a HCI_User_Confirmation_Request_Reply command is received.
  void OnUserConfirmationRequestReplyCommand(
      const hci_spec::UserConfirmationRequestReplyCommandParams& params);

  // Called when a HCI_User_Confirmation_Request_Negative_Reply command is received.
  void OnUserConfirmationRequestNegativeReplyCommand(
      const hci_spec::UserConfirmationRequestNegativeReplyCommandParams& params);

  // Called when a HCI_Set_Connection_Encryption command is received.
  void OnSetConnectionEncryptionCommand(
      const hci_spec::SetConnectionEncryptionCommandParams& params);

  // Called when a HCI_Read_Encryption_Key_Size command is received.
  void OnReadEncryptionKeySizeCommand(const hci_spec::ReadEncryptionKeySizeParams& params);

  // Called when a HCI_Enhanced_Accept_Synchronous_Connection_Request command is received.
  void OnEnhancedAcceptSynchronousConnectionRequestCommand(
      const hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandParams& params);

  // Called when a HCI_Enhanced_Setup_Synchronous_Connection command is received.
  void OnEnhancedSetupSynchronousConnectionCommand(
      const hci_spec::EnhancedSetupSynchronousConnectionCommandParams& params);

  // Called when a HCI_LE_Read_Remote_Features_Command is received.
  void OnLEReadRemoteFeaturesCommand(const hci_spec::LEReadRemoteFeaturesCommandParams& params);

  // Called when a HCI_LE_Enable_Encryption command is received, responds with a successful
  // encryption change event.
  void OnLEStartEncryptionCommand(const hci_spec::LEStartEncryptionCommandParams& params);

  // Called when a command with an OGF of hci_spec::kVendorOGF is received.
  void OnVendorCommand(const PacketView<hci_spec::CommandHeader>& command_packet);

  // Respond to a command packet. This may be done immediately upon reception or via a client-
  // triggered callback if pause_responses_for_opcode has been called for that command's opcode.
  void HandleReceivedCommandPacket(const PacketView<hci_spec::CommandHeader>& command_packet);

  // ControllerTestDoubleBase overrides:
  void OnCommandPacketReceived(const PacketView<hci_spec::CommandHeader>& command_packet) override;
  void OnACLDataPacketReceived(const ByteBuffer& acl_data_packet) override;

  Settings settings_;
  LEScanState le_scan_state_;
  LEAdvertisingState legacy_advertising_state_;
  std::unordered_map<hci_spec::AdvertisingHandle, LEAdvertisingState> extended_advertising_states_;

  // Used for BR/EDR Scans
  uint8_t bredr_scan_state_;
  hci_spec::PageScanType page_scan_type_ = hci_spec::PageScanType::kStandardScan;
  uint16_t page_scan_interval_ = 0x800;
  uint16_t page_scan_window_ = 0x0012;

  // The GAP local name, as written/read by HCI_(Read/Write)_Local_Name. While the aforementioned
  // HCI commands carry the name in a 248 byte buffer, |local_name_| contains the intended value.
  std::string local_name_;

  // The local device class configured by HCI_Write_Class_of_Device.
  DeviceClass device_class_;

  // Variables used for
  // HCI_LE_Create_Connection/HCI_LE_Create_Connection_Cancel.
  uint16_t next_conn_handle_ = 0u;
  fxl::CancelableClosure pending_le_connect_rsp_;
  std::optional<LEConnectParams> le_connect_params_;
  bool le_connect_pending_ = false;

  // Variables used for
  // HCI_BREDR_Create_Connection/HCI_BREDR_Create_Connection_Cancel.
  bool bredr_connect_pending_ = false;
  DeviceAddress pending_bredr_connect_addr_;
  fxl::CancelableClosure pending_bredr_connect_rsp_;

  // ID used for L2CAP LE signaling channel commands.
  uint8_t next_le_sig_id_ = 1u;

  // Used to indicate whether to respond back to TX Power Level read or not.
  bool respond_to_tx_power_read_ = true;

  // The Inquiry Mode that the controller is in.  Determines what types of
  // events are faked when a hci_spec::kInquiry is started.
  hci_spec::InquiryMode inquiry_mode_;

  // The maximum number of advertising sets supported by the controller
  uint8_t num_supported_advertising_sets_ = 1;

  // The number of results left in Inquiry Mode operation.
  // If negative, no limit has been set.
  int16_t inquiry_num_responses_left_;

  // Used to setup default Command Status event responses.
  std::unordered_map<hci_spec::OpCode, hci_spec::StatusCode> default_command_status_map_;

  // Used to setup default Command Complete event status responses (for simulating errors)
  std::unordered_map<hci_spec::OpCode, hci_spec::StatusCode> default_status_map_;

  // The set of fake peers that are visible.
  std::unordered_map<DeviceAddress, std::unique_ptr<FakePeer>> peers_;

  // Callbacks and counters that are intended for unit tests.
  int le_create_connection_command_count_ = 0;
  int acl_create_connection_command_count_ = 0;

  fit::function<void(hci_spec::LECreateConnectionCommandParams)> le_create_connection_cb_;
  fit::closure controller_parameters_cb_;
  ScanStateCallback scan_state_cb_;
  fit::closure advertising_state_cb_;
  ConnectionStateCallback conn_state_cb_;
  LEConnectionParametersCallback le_conn_params_cb_;
  fit::closure le_read_remote_features_cb_;
  VendorCommandCallback vendor_command_cb_;

  // Associates opcodes with client-supplied pause listeners. Commands with these opcodes will hang
  // with no response until the client invokes the passed-out closure.
  std::unordered_map<hci_spec::OpCode, fit::function<void(fit::closure)>> paused_opcode_listeners_;

  // Called when ACL data packets received.
  DataCallback data_callback_ = nullptr;
  async_dispatcher_t* data_dispatcher_ = nullptr;

  bool auto_completed_packets_event_enabled_ = true;
  bool auto_disconnection_complete_event_enabled_ = true;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<FakeController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeController);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_CONTROLLER_H_
