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

#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/bredr_connection_request.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::testing {

namespace android_hci = bt::hci_spec::vendor::android;
namespace android_emb = pw::bluetooth::vendor::android_hci;

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kAcceptConnectionRequest),
      UpperBits(hci_spec::kAcceptConnectionRequest),
      0x07,  // parameter_total_size (7 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      0x00  // Role (Become central)
      ));
}

DynamicByteBuffer AuthenticationRequestedPacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kAuthenticationRequested),
      UpperBits(hci_spec::kAuthenticationRequested),
      0x02,             // parameter_total_size (2 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn)   // Connection_Handle
      ));
}

DynamicByteBuffer CommandCompletePacket(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x04,  // parameter_total_size (4 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(opcode),  // Command_Opcode
      UpperBits(opcode),  // Command_Opcode
      status              // Status
      ));
}

DynamicByteBuffer CommandStatusPacket(
    hci_spec::OpCode op_code,
    pw::bluetooth::emboss::StatusCode status_code,
    uint8_t num_packets) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandStatusEventCode,
      0x04,         // parameter_total_size (4 bytes)
      status_code,  // Status
      num_packets,  // Num_HCI_Command_Packets allowed to be sent to controller
      LowerBits(op_code),  // Command_Opcode
      UpperBits(op_code)   // Command_Opcode
      ));
}

DynamicByteBuffer ConnectionCompletePacket(
    DeviceAddress address,
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode status) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kConnectionCompleteEventCode,
      0x0B,             // parameter_total_size (11 bytes)
      status,           // Status
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      0x01,  // Link_Type (ACL)
      0x00   // Encryption_Enabled (Disabled)
      ));
}

DynamicByteBuffer ConnectionRequestPacket(DeviceAddress address,
                                          hci_spec::LinkType link_type) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kConnectionRequestEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      0x00,      // Class_Of_Device (Unknown)
      0x1F,      // Class_Of_Device (Unknown)
      0x00,      // Class_Of_Device (Unknown)
      link_type  // Link_Type
      ));
}

DynamicByteBuffer CreateConnectionPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kCreateConnection),
      UpperBits(hci_spec::kCreateConnection),
      0x0d,  // parameter_total_size (13 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      LowerBits(hci::kEnableAllPacketTypes),  // Packet_Type
      UpperBits(hci::kEnableAllPacketTypes),  // Packet_Type
      0x02,                                   // Page_Scan_Repetition_Mode (R2)
      0x00,                                   // Reserved
      0x00,                                   // Clock_Offset
      0x00,                                   // Clock_Offset
      0x00                                    // Allow_Role_Switch (Not allowed)
      ));
}

DynamicByteBuffer DisconnectionCompletePacket(
    hci_spec::ConnectionHandle conn, pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kDisconnectionCompleteEventCode,
      0x04,  // parameter_total_size (4 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      LowerBits(conn),                             // Connection_Handle
      UpperBits(conn),                             // Connection_Handle
      reason                                       // Reason
      ));
}

DynamicByteBuffer DisconnectPacket(hci_spec::ConnectionHandle conn,
                                   pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kDisconnect),
      UpperBits(hci_spec::kDisconnect),
      0x03,             // parameter_total_size (3 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      reason            // Reason
      ));
}

DynamicByteBuffer DisconnectStatusResponsePacket() {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandStatusEventCode,
      0x04,  // parameter_total_size (3 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kDisconnect),  // Command_Opcode
      UpperBits(hci_spec::kDisconnect)   // Command_Opcode
      ));
}

DynamicByteBuffer EmptyCommandPacket(hci_spec::OpCode opcode) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(opcode), UpperBits(opcode), /*length=*/0));
}

DynamicByteBuffer EncryptionChangeEventPacket(
    pw::bluetooth::emboss::StatusCode status_code,
    hci_spec::ConnectionHandle conn,
    hci_spec::EncryptionStatus encryption_enabled) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kEncryptionChangeEventCode,
      0x04,             // parameter_total_size (4 bytes)
      status_code,      // Status
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      static_cast<uint8_t>(encryption_enabled)  // Encryption_Enabled
      ));
}

DynamicByteBuffer EnhancedAcceptSynchronousConnectionRequestPacket(
    DeviceAddress peer_address,
    bt::StaticPacket<
        pw::bluetooth::emboss::SynchronousConnectionParametersWriter> params) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::
          EnhancedAcceptSynchronousConnectionRequestCommandWriter>(
      hci_spec::kEnhancedAcceptSynchronousConnectionRequest);
  auto view = packet.view_t();

  view.bd_addr().CopyFrom(peer_address.value().view());
  view.connection_parameters().CopyFrom(params.view());

  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer EnhancedSetupSynchronousConnectionPacket(
    hci_spec::ConnectionHandle conn,
    bt::StaticPacket<
        pw::bluetooth::emboss::SynchronousConnectionParametersWriter> params) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::EnhancedSetupSynchronousConnectionCommandWriter>(
      hci_spec::kEnhancedSetupSynchronousConnection);

  auto view = packet.view_t();
  view.connection_handle().Write(conn);
  view.connection_parameters().CopyFrom(params.view());

  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer InquiryCommandPacket(uint16_t inquiry_length) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kInquiry),
      UpperBits(hci_spec::kInquiry),
      0x05,            // parameter_total_size (5 bytes)
      0x33,            // LAP (GIAC)
      0x8B,            // LAP (GIAC)
      0x9E,            // LAP (GIAC)
      inquiry_length,  // Inquiry_Length
      0x00             // Num_Responses (Unlimited)
      ));
}

DynamicByteBuffer InquiryCommandResponse(
    pw::bluetooth::emboss::StatusCode status_code) {
  return CommandStatusPacket(hci_spec::kInquiry, status_code);
}

DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::LESupportedFeatures le_features) {
  const BufferView features(&le_features, sizeof(le_features));
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kLEMetaEventCode,
      0x0c,  // parameter_total_size (12 bytes)
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,  // Subevent_Code
      pw::bluetooth::emboss::StatusCode::SUCCESS,           // Status
      LowerBits(conn),                                      // Connection_Handle
      UpperBits(conn),                                      // Connection_Handle
      // LE_Features (8 bytes)
      features[0],
      features[1],
      features[2],
      features[3],
      features[4],
      features[5],
      features[6],
      features[7]));
}

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kLEReadRemoteFeatures),
      UpperBits(hci_spec::kLEReadRemoteFeatures),
      0x02,             // parameter_total_size (2 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn)   // Connection_Handle
      ));
}

DynamicByteBuffer LECisRequestEventPacket(
    hci_spec::ConnectionHandle acl_connection_handle,
    hci_spec::ConnectionHandle cis_connection_handle,
    hci_spec::CigIdentifier cig_id,
    hci_spec::CisIdentifier cis_id) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kLEMetaEventCode,
      0x07,                                 // parameter_total_size (7 bytes)
      hci_spec::kLECISRequestSubeventCode,  // Subevent_Code
      LowerBits(acl_connection_handle),     // ACL_Connection_Handle
      UpperBits(acl_connection_handle),     // ACL_Connection_Handle
      LowerBits(cis_connection_handle),     // CIS_Connection_Handle
      UpperBits(cis_connection_handle),     // CIS_Connection_Handle
      cig_id,                               // CIG_ID
      cis_id)                               // CIS_ID
  );
}

DynamicByteBuffer LEAcceptCisRequestCommandPacket(
    hci_spec::ConnectionHandle cis_handle) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLEAcceptCISRequest),
                       UpperBits(hci_spec::kLEAcceptCISRequest),
                       0x02,
                       LowerBits(cis_handle),
                       UpperBits(cis_handle)));
}

DynamicByteBuffer LERejectCisRequestCommandPacket(
    hci_spec::ConnectionHandle cis_handle,
    pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLERejectCISRequest),
                       UpperBits(hci_spec::kLERejectCISRequest),
                       0x03,                   // parameter_total_size (3 bytes)
                       LowerBits(cis_handle),  // Connection_Handle
                       UpperBits(cis_handle),  // Connection_Handle
                       reason)                 // Reason
  );
}

DynamicByteBuffer LERequestPeerScaCompletePacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::LESleepClockAccuracyRange sca) {
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LERequestPeerSCACompleteSubeventWriter>(
      hci_spec::kLEMetaEventCode);

  auto view = packet.view_t();
  view.le_meta_event().subevent_code().Write(
      hci_spec::kLERequestPeerSCACompleteSubeventCode);
  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);
  view.connection_handle().Write(conn);
  view.peer_clock_accuracy().Write(sca);

  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer LECisEstablishedEventPacket(
    pw::bluetooth::emboss::StatusCode status,
    hci_spec::ConnectionHandle connection_handle,
    uint32_t cig_sync_delay_us,
    uint32_t cis_sync_delay_us,
    uint32_t transport_latency_c_to_p_us,
    uint32_t transport_latency_p_to_c_us,
    pw::bluetooth::emboss::IsoPhyType phy_c_to_p,
    pw::bluetooth::emboss::IsoPhyType phy_p_to_c,
    uint8_t nse,
    uint8_t bn_c_to_p,
    uint8_t bn_p_to_c,
    uint8_t ft_c_to_p,
    uint8_t ft_p_to_c,
    uint16_t max_pdu_c_to_p,
    uint16_t max_pdu_p_to_c,
    uint16_t iso_interval) {
  auto packet = hci::EmbossEventPacket::New<
      pw::bluetooth::emboss::LECISEstablishedSubeventWriter>(
      hci_spec::kLEMetaEventCode);
  auto view = packet.view_t();

  view.le_meta_event().subevent_code().Write(
      hci_spec::kLECISEstablishedSubeventCode);
  view.status().Write(status);
  view.connection_handle().Write(connection_handle);
  view.cig_sync_delay().Write(cig_sync_delay_us);
  view.cis_sync_delay().Write(cis_sync_delay_us);
  view.transport_latency_c_to_p().Write(transport_latency_c_to_p_us);
  view.transport_latency_p_to_c().Write(transport_latency_p_to_c_us);
  view.phy_c_to_p().Write(phy_c_to_p);
  view.phy_p_to_c().Write(phy_p_to_c);
  view.nse().Write(nse);
  view.bn_c_to_p().Write(bn_c_to_p);
  view.bn_p_to_c().Write(bn_p_to_c);
  view.ft_c_to_p().Write(ft_c_to_p);
  view.ft_p_to_c().Write(ft_p_to_c);
  view.max_pdu_c_to_p().Write(max_pdu_c_to_p);
  view.max_pdu_p_to_c().Write(max_pdu_p_to_c);
  view.iso_interval().Write(iso_interval);

  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer LERequestPeerScaPacket(hci_spec::ConnectionHandle conn) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LERequestPeerSCACommandWriter>(
      hci_spec::kLERequestPeerSCA);
  auto view = packet.view_t();
  view.connection_handle().Write(conn);
  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle conn,
                                          uint64_t random_number,
                                          uint16_t encrypted_diversifier,
                                          UInt128 ltk) {
  const BufferView rand(&random_number, sizeof(random_number));
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kLEStartEncryption),
      UpperBits(hci_spec::kLEStartEncryption),
      0x1c,             // parameter_total_size (28 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      // Random_Number (8 bytes)
      rand[0],
      rand[1],
      rand[2],
      rand[3],
      rand[4],
      rand[5],
      rand[6],
      rand[7],
      LowerBits(encrypted_diversifier),  // Encrypted_Diversifier
      UpperBits(encrypted_diversifier),  // Encrypted_Diversifier
      // Long_Term_Key (16 bytes)
      ltk[0],
      ltk[1],
      ltk[2],
      ltk[3],
      ltk[4],
      ltk[5],
      ltk[6],
      ltk[7],
      ltk[8],
      ltk[9],
      ltk[10],
      ltk[11],
      ltk[12],
      ltk[13],
      ltk[14],
      ltk[15]));
}

DynamicByteBuffer NumberOfCompletedPacketsPacket(
    hci_spec::ConnectionHandle conn, uint16_t num_packets) {
  return DynamicByteBuffer(StaticByteBuffer(
      0x13,
      0x05,  // Number Of Completed Packet HCI event header, parameters length
      0x01,  // Num_Handles
      LowerBits(conn),         // Connection_Handle
      UpperBits(conn),         // Connection_Handle
      LowerBits(num_packets),  // Num_Completed_Packets
      UpperBits(num_packets)   // Num_Completed_Packets
      ));
}

DynamicByteBuffer ReadRemoteExtended1CompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,  // parameter_total_size (13 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      LowerBits(conn),                             // Connection_Handle
      UpperBits(conn),                             // Connection_Handle
      0x01,                                        // Page_Number
      0x03,                                        // Max_Page_Number (3 pages)
      // Extended_LMP_Features (8 bytes)
      0x0F,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00
      // lmp_features_page_1: Secure Simple Pairing (Host Support), LE Supported
      // (Host), SimultaneousLEAndBREDR, Secure Connections (Host Support)
      ));
}

DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,             // parameter_total_size (3 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      0x01              // Page_Number (1)
      ));
}

DynamicByteBuffer ReadRemoteExtended2CompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,  // parameter_total_size (13 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      LowerBits(conn),                             // Connection_Handle
      UpperBits(conn),                             // Connection_Handle
      0x02,                                        // Page_Number
      0x03,                                        // Max_Page_Number (3 pages)
      // Extended_LMP_Features (8 bytes)
      0x00,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0xFF,
      0x00
      // lmp_features_page2: All the bits should be ignored
      ));
}

DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,             // parameter_total_size (3 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      0x02              // Page_Number (2)
      ));
}

DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteVersionInfoCompleteEventCode,
      0x08,  // parameter_total_size (8 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      LowerBits(conn),                             // Connection_Handle
      UpperBits(conn),                             // Connection_Handle
      pw::bluetooth::emboss::CoreSpecificationVersion::V4_2,  // Version
      0xE0,  // Company_Identifier (Google)
      0x00,  // Company_Identifier (Google)
      0xAD,  // Subversion (Anything)
      0xDE   // Subversion (Anything)
      ));
}

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteVersionInfo),
      UpperBits(hci_spec::kReadRemoteVersionInfo),
      0x02,             // parameter_total_size (2 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn)   // Connection_Handle
      ));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, bool extended_features) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
      0x0B,  // parameter_total_size (11 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      LowerBits(conn),                             // Connection_Handle
      UpperBits(conn),                             // Connection_Handle
      // LMP_Features (8 bytes)
      0xFF,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      (extended_features ? 0x80 : 0x00)
      // lmp_features_page0: 3 slot packets, 5 slot packets, Encryption,
      // Timing Accuracy, Role Switch, Hold Mode, Sniff Mode, LE Supported,
      // Extended Features if enabled
      ));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteSupportedFeatures),
      UpperBits(hci_spec::kReadRemoteSupportedFeatures),
      0x02,             // parameter_total_size (2 bytes)
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn)   // Connection_Handle
      ));
}

DynamicByteBuffer ReadScanEnable() {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadScanEnable),
      UpperBits(hci_spec::kReadScanEnable),
      0x00  // No parameters
      ));
}

DynamicByteBuffer ReadScanEnableResponse(uint8_t scan_enable) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x05,  // parameter_total_size (5 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kReadScanEnable),
      UpperBits(hci_spec::kReadScanEnable),
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      scan_enable                                  // Scan_Enable
      ));
}

DynamicByteBuffer RejectConnectionRequestPacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode reason) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRejectConnectionRequest),
      UpperBits(hci_spec::kRejectConnectionRequest),
      0x07,  // parameter_total_size (7 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      reason  // Reason
      ));
}
DynamicByteBuffer RejectSynchronousConnectionRequest(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRejectSynchronousConnectionRequest),
      UpperBits(hci_spec::kRejectSynchronousConnectionRequest),
      0x07,  // parameter_total_size (7 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      status_code  // Reason
      ));
}

DynamicByteBuffer RemoteNameRequestCompletePacket(DeviceAddress address,
                                                  const std::string& name) {
  auto addr = address.value().bytes();
  auto event = DynamicByteBuffer(
      pw::bluetooth::emboss::RemoteNameRequestCompleteEventView::
          IntrinsicSizeInBytes()
              .Read());
  event.SetToZeros();
  const StaticByteBuffer header(
      hci_spec::kRemoteNameRequestCompleteEventCode,
      0xff,  // parameter_total_size (255 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]);
  header.Copy(&event);
  event.Write(reinterpret_cast<const uint8_t*>(name.data()),
              name.size(),
              header.size());
  return event;
}

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRemoteNameRequest),
      UpperBits(hci_spec::kRemoteNameRequest),
      0x0a,  // parameter_total_size (10 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      0x00,  // Page_Scan_Repetition_Mode (R0)
      0x00,  // Reserved
      0x00,  // Clock_Offset
      0x00   // Clock_Offset
      ));
}

DynamicByteBuffer RoleChangePacket(DeviceAddress address,
                                   pw::bluetooth::emboss::ConnectionRole role,
                                   pw::bluetooth::emboss::StatusCode status) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kRoleChangeEventCode,
      0x08,    // parameter_total_size (8 bytes)
      status,  // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      role  // Role
      ));
}

DynamicByteBuffer ScoDataPacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::SynchronousDataPacketStatusFlag flag,
    const BufferView& payload,
    std::optional<uint8_t> payload_length_override) {
  // Flag is bits 4-5 in the higher octet of |handle_and_flags|, i.e.
  // 0b00xx000000000000.
  conn |= static_cast<uint8_t>(flag) << 12;
  StaticByteBuffer header(
      LowerBits(conn),
      UpperBits(conn),
      payload_length_override ? *payload_length_override : payload.size());
  DynamicByteBuffer out(header.size() + payload.size());
  header.Copy(&out);
  MutableBufferView payload_view = out.mutable_view(header.size());
  payload.Copy(&payload_view);
  return out;
}

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn,
                                          bool enable) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kSetConnectionEncryption),
      UpperBits(hci_spec::kSetConnectionEncryption),
      0x03,                         // parameter_total_size (3 bytes)
      LowerBits(conn),              // Connection_Handle
      UpperBits(conn),              // Connection_Handle
      static_cast<uint8_t>(enable)  // Encryption_Enable
      ));
}

DynamicByteBuffer StartA2dpOffloadRequest(
    const l2cap::A2dpOffloadManager::Configuration& config,
    hci_spec::ConnectionHandle connection_handle,
    l2cap::ChannelId l2cap_channel_id,
    uint16_t l2cap_mtu_size) {
  constexpr size_t kPacketSize =
      android_emb::StartA2dpOffloadCommand::MaxSizeInBytes();
  auto packet =
      hci::EmbossCommandPacket::New<android_emb::StartA2dpOffloadCommandWriter>(
          android_hci::kA2dpOffloadCommand, kPacketSize);
  auto view = packet.view_t();

  view.vendor_command().sub_opcode().Write(
      android_hci::kStartA2dpOffloadCommandSubopcode);
  view.codec_type().Write(config.codec);
  view.max_latency().Write(config.max_latency);
  view.scms_t_enable().CopyFrom(
      const_cast<l2cap::A2dpOffloadManager::Configuration&>(config)
          .scms_t_enable.view());
  view.sampling_frequency().Write(config.sampling_frequency);
  view.bits_per_sample().Write(config.bits_per_sample);
  view.channel_mode().Write(config.channel_mode);
  view.encoded_audio_bitrate().Write(config.encoded_audio_bit_rate);
  view.connection_handle().Write(connection_handle);
  view.l2cap_channel_id().Write(l2cap_channel_id);
  view.l2cap_mtu_size().Write(l2cap_mtu_size);

  switch (config.codec) {
    case android_emb::A2dpCodecType::SBC:
      view.sbc_codec_information().CopyFrom(
          const_cast<l2cap::A2dpOffloadManager::Configuration&>(config)
              .sbc_configuration.view());
      break;
    case android_emb::A2dpCodecType::AAC:
      view.aac_codec_information().CopyFrom(
          const_cast<l2cap::A2dpOffloadManager::Configuration&>(config)
              .aac_configuration.view());
      break;
    case android_emb::A2dpCodecType::LDAC:
      view.ldac_codec_information().CopyFrom(
          const_cast<l2cap::A2dpOffloadManager::Configuration&>(config)
              .ldac_configuration.view());
      break;
    default:
      break;
  }

  return packet.release();
}

DynamicByteBuffer StopA2dpOffloadRequest() {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(android_hci::kA2dpOffloadCommand),
                       UpperBits(android_hci::kA2dpOffloadCommand),
                       0x01,  // parameter_total_size (1 byte)
                       android_hci::kStopA2dpOffloadCommandSubopcode));
}

DynamicByteBuffer SynchronousConnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    DeviceAddress address,
    hci_spec::LinkType link_type,
    pw::bluetooth::emboss::StatusCode status) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kSynchronousConnectionCompleteEventCode,
      0x11,             // parameter_total_size (17 bytes)
      status,           // Status
      LowerBits(conn),  // Connection_Handle
      UpperBits(conn),  // Connection_Handle
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      link_type,  // Link_Type
      0x00,       // Transmission_Interval interval
      0x00,       // Retransmission_Window
      0x00,       // RX_Packet_Length
      0x00,       // RX_Packet_Length
      0x00,       // TX_Packet_Length
      0x00,       // TX_Packet_Length
      0x00        // Air_Mode
      ));
}

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(
    hci_spec::ConnectionHandle conn, uint16_t flush_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWriteAutomaticFlushTimeout),
      UpperBits(hci_spec::kWriteAutomaticFlushTimeout),
      0x04,                      // parameter_total_size (4 bytes)
      LowerBits(conn),           // Connection_Handle
      UpperBits(conn),           // Connection_Handle
      LowerBits(flush_timeout),  // Flush_Timeout
      UpperBits(flush_timeout)   // Flush_Timeout
      ));
}

DynamicByteBuffer WriteExtendedInquiryResponse(
    pw::bluetooth::emboss::StatusCode status_code) {
  return CommandCompletePacket(hci_spec::kWriteExtendedInquiryResponse,
                               status_code);
}

DynamicByteBuffer WriteInquiryScanActivity(uint16_t scan_interval,
                                           uint16_t scan_window) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWriteInquiryScanActivity),
      UpperBits(hci_spec::kWriteInquiryScanActivity),
      0x04,                      // parameter_total_size (4 bytes)
      LowerBits(scan_interval),  // Inquiry_Scan_Interval
      UpperBits(scan_interval),  // Inquiry_Scan_Interval
      LowerBits(scan_window),    // Inquiry_Scan_Window
      UpperBits(scan_window)     // Inquiry_Scan_Window
      ));
}

DynamicByteBuffer WriteInquiryScanActivityResponse() {
  return CommandCompletePacket(hci_spec::kWriteInquiryScanActivity,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
}

DynamicByteBuffer WriteLocalNameResponse(
    pw::bluetooth::emboss::StatusCode status_code) {
  return CommandCompletePacket(hci_spec::kWriteLocalName, status_code);
}

DynamicByteBuffer WritePageScanActivityPacket(uint16_t scan_interval,
                                              uint16_t scan_window) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePageScanActivity),
      UpperBits(hci_spec::kWritePageScanActivity),
      0x04,                      // parameter_total_size (4 bytes)
      LowerBits(scan_interval),  // Page_Scan_Interval
      UpperBits(scan_interval),  // Page_Scan_Interval
      LowerBits(scan_window),    // Page_Scan_Window
      UpperBits(scan_window)     // Page_Scan_Window
      ));
}

DynamicByteBuffer WritePageScanActivityResponse() {
  return CommandCompletePacket(hci_spec::kWritePageScanActivity,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
}

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePageTimeout),
      UpperBits(hci_spec::kWritePageTimeout),
      0x02,                     // parameter_total_size (2 bytes)
      LowerBits(page_timeout),  // Page_Timeout
      UpperBits(page_timeout)   // Page_Timeout
      ));
}

DynamicByteBuffer WriteScanEnable(uint8_t scan_enable) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWriteScanEnable),
      UpperBits(hci_spec::kWriteScanEnable),
      0x01,        // parameter_total_size (1 byte)
      scan_enable  // Scan_Enable
      ));
}

DynamicByteBuffer WriteScanEnableResponse() {
  return CommandCompletePacket(hci_spec::kWriteScanEnable,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
}

}  // namespace bt::testing
