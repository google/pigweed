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
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/bredr_connection_request.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_string/string_builder.h"

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
  const auto addr = address.value().bytes();
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
  const auto addr = address.value().bytes();
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

DynamicByteBuffer CreateConnectionCancelPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kCreateConnectionCancel),
                       UpperBits(hci_spec::kCreateConnectionCancel),
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
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
  auto packet = hci::CommandPacket::New<
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
  auto packet = hci::CommandPacket::New<
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

DynamicByteBuffer IoCapabilityRequestNegativeReplyPacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kIOCapabilityRequestNegativeReply),
      UpperBits(hci_spec::kIOCapabilityRequestNegativeReply),
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

DynamicByteBuffer IoCapabilityRequestNegativeReplyResponse(
    DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kIOCapabilityRequestNegativeReply),  // Command_Opcode
      UpperBits(hci_spec::kIOCapabilityRequestNegativeReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,              // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer IoCapabilityRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kIOCapabilityRequestEventCode,
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer IoCapabilityRequestReplyPacket(
    DeviceAddress address,
    pw::bluetooth::emboss::IoCapability io_cap,
    pw::bluetooth::emboss::AuthenticationRequirements auth_req) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kIOCapabilityRequestReply),
      UpperBits(hci_spec::kIOCapabilityRequestReply),
      0x09,  // parameter_total_size (9 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      io_cap,   // IO_Capability
      0x00,     // OOB_Data_Present (Not present)
      auth_req  // Authentication_Requirements
      ));
}

DynamicByteBuffer IoCapabilityRequestReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kIOCapabilityRequestReply),  // Command_Opcode
      UpperBits(hci_spec::kIOCapabilityRequestReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,      // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer IoCapabilityResponsePacket(
    DeviceAddress address,
    pw::bluetooth::emboss::IoCapability io_cap,
    pw::bluetooth::emboss::AuthenticationRequirements auth_req) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kIOCapabilityResponseEventCode,
      0x09,  // parameter_total_size (9 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      io_cap,   // IO_Capability
      0x00,     // OOB_Data_Present (Not present)
      auth_req  // Authentication_Requirements
      ));
}

DynamicByteBuffer IsoDataPacket(size_t frame_total_size,
                                hci_spec::ConnectionHandle connection_handle,
                                uint16_t packet_sequence_number) {
  DynamicByteBuffer packet(frame_total_size);
  auto view = pw::bluetooth::emboss::MakeIsoDataFramePacketView(
      packet.mutable_data(), frame_total_size);

  const size_t data_total_length =
      frame_total_size - view.header().IntrinsicSizeInBytes().Read();
  view.header().connection_handle().Write(connection_handle);
  view.header().pb_flag().Write(
      pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU);
  view.header().ts_flag().Write(
      pw::bluetooth::emboss::TsFlag::TIMESTAMP_NOT_PRESENT);
  view.header().data_total_length().Write(data_total_length);

  const size_t iso_sdu_length =
      frame_total_size - view.sdu_fragment_offset().Read();
  view.packet_sequence_number().Write(packet_sequence_number);
  view.iso_sdu_length().Write(iso_sdu_length);
  view.packet_status_flag().Write(
      pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA);

  // Write payload
  for (size_t n = 0; n < iso_sdu_length; n++) {
    view.iso_sdu_fragment()[n].Write(static_cast<uint8_t>(n * 2));
  }

  return packet;
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
  auto packet = hci::EventPacket::New<
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
  auto packet = hci::EventPacket::New<
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

DynamicByteBuffer LESetupIsoDataPathPacket(
    hci_spec::ConnectionHandle connection_handle,
    pw::bluetooth::emboss::DataPathDirection direction,
    uint8_t data_path_id,
    bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter> codec_id,
    uint32_t controller_delay,
    const std::optional<std::vector<uint8_t>>& codec_configuration) {
  size_t packet_size =
      pw::bluetooth::emboss::LESetupISODataPathCommand::MinSizeInBytes() +
      (codec_configuration.has_value() ? codec_configuration->size() : 0);
  auto packet = hci::CommandPacket::New<
      pw::bluetooth::emboss::LESetupISODataPathCommandWriter>(
      hci_spec::kLESetupISODataPath, packet_size);
  auto view = packet.view_t();
  view.connection_handle().Write(connection_handle);
  view.data_path_direction().Write(direction);
  view.data_path_id().Write(data_path_id);
  view.codec_id().CopyFrom(
      const_cast<bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter>&>(
          codec_id)
          .view());
  view.controller_delay().Write(controller_delay);
  if (codec_configuration.has_value()) {
    view.codec_configuration_length().Write(codec_configuration->size());
    std::memcpy(view.codec_configuration().BackingStorage().data(),
                codec_configuration->data(),
                codec_configuration->size());
  } else {
    view.codec_configuration_length().Write(0);
  }
  return DynamicByteBuffer(packet.data());
}

DynamicByteBuffer LESetupIsoDataPathResponse(
    pw::bluetooth::emboss::StatusCode status,
    hci_spec::ConnectionHandle connection_handle) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x06,  // parameter_total_size (6 bytes)
      0xF0,  // Num_HCI_CommandPackets allowed to be sent to controller (240)
      LowerBits(hci_spec::kLESetupISODataPath),
      UpperBits(hci_spec::kLESetupISODataPath),
      status,
      LowerBits(connection_handle),
      UpperBits(connection_handle)));
}

DynamicByteBuffer LERequestPeerScaPacket(hci_spec::ConnectionHandle conn) {
  auto packet = hci::CommandPacket::New<
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

DynamicByteBuffer LinkKeyNotificationPacket(DeviceAddress address,
                                            UInt128 link_key,
                                            hci_spec::LinkKeyType key_type) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kLinkKeyNotificationEventCode,
      0x17,  // parameter_total_size (23 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      // Link_Key (16 bytes)
      link_key[0],
      link_key[1],
      link_key[2],
      link_key[3],
      link_key[4],
      link_key[5],
      link_key[6],
      link_key[7],
      link_key[8],
      link_key[9],
      link_key[10],
      link_key[11],
      link_key[12],
      link_key[13],
      link_key[14],
      link_key[15],
      key_type  // Key_Type
      ));
}

DynamicByteBuffer LinkKeyRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kLinkKeyRequestEventCode,
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer LinkKeyRequestNegativeReplyPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLinkKeyRequestNegativeReply),
                       UpperBits(hci_spec::kLinkKeyRequestNegativeReply),
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer LinkKeyRequestNegativeReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kLinkKeyRequestNegativeReply),  // Command_Opcode
      UpperBits(hci_spec::kLinkKeyRequestNegativeReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,         // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer LinkKeyRequestReplyPacket(DeviceAddress address,
                                            UInt128 link_key) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLinkKeyRequestReply),
                       UpperBits(hci_spec::kLinkKeyRequestReply),
                       0x16,  // parameter_total_size (22 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5],
                       // Link_Key (16 bytes)
                       link_key[0],
                       link_key[1],
                       link_key[2],
                       link_key[3],
                       link_key[4],
                       link_key[5],
                       link_key[6],
                       link_key[7],
                       link_key[8],
                       link_key[9],
                       link_key[10],
                       link_key[11],
                       link_key[12],
                       link_key[13],
                       link_key[14],
                       link_key[15]));
}

DynamicByteBuffer LinkKeyRequestReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kLinkKeyRequestReply),   // Command_Opcode
      UpperBits(hci_spec::kLinkKeyRequestReply),   // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
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

DynamicByteBuffer PinCodeRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kPinCodeRequestEventCode,
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer PinCodeRequestNegativeReplyPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kPinCodeRequestNegativeReply),
                       UpperBits(hci_spec::kPinCodeRequestNegativeReply),
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer PinCodeRequestNegativeReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kPinCodeRequestNegativeReply),  // Command_Opcode
      UpperBits(hci_spec::kPinCodeRequestNegativeReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,         // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer PinCodeRequestReplyPacket(DeviceAddress address,
                                            uint8_t pin_length,
                                            std::string pin_code) {
  const auto addr = address.value().bytes();

  StaticByteBuffer<16> pin;
  pin.Write(BufferView(pin_code));

  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kPinCodeRequestReply),
                       UpperBits(hci_spec::kPinCodeRequestReply),
                       0x17,  // parameter_total_size (23 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5],
                       pin_length,  // PIN_Code_Length
                       // PIN_Code
                       pin[0],
                       pin[1],
                       pin[2],
                       pin[3],
                       pin[4],
                       pin[5],
                       pin[6],
                       pin[7],
                       pin[8],
                       pin[9],
                       pin[10],
                       pin[11],
                       pin[12],
                       pin[13],
                       pin[14],
                       pin[15]));
}

DynamicByteBuffer PinCodeRequestReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kPinCodeRequestReply),   // Command_Opcode
      UpperBits(hci_spec::kPinCodeRequestReply),   // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
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
      0x00,
      0x00,
      0x00,
      0x00
      // lmp_features_page1: Secure Simple Pairing (Host Support), LE Supported
      // (Host), Previously used, Secure Connections (Host Support)
      ));
}

DynamicByteBuffer ReadRemoteExtended1CompletePacketNoSsp(
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
      0x0E,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00
      // lmp_features_page1: LE Supported (Host), Previously used, Secure
      // Connections (Host Support)
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
      0x04,
      0x00,
      0x08,
      (extended_features ? 0x80 : 0x00)
      // lmp_features_page0: 3 slot packets, 5 slot packets, Encryption, Slot
      // Offset, Timing Accuracy, Role Switch, Hold Mode, Sniff Mode, LE
      // Supported (Controller), Secure Simple Pairing (Controller Support),
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
      LowerBits(hci_spec::kReadScanEnable),        // Command_Opcode
      UpperBits(hci_spec::kReadScanEnable),        // Command_Opcode
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
  const auto addr = address.value().bytes();
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
  const auto addr = address.value().bytes();
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

DynamicByteBuffer SimplePairingCompletePacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kSimplePairingCompleteEventCode,
                       0x07,  // parameter_total_size (7 bytes)
                       status_code,
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer StartA2dpOffloadRequest(
    const l2cap::A2dpOffloadManager::Configuration& config,
    hci_spec::ConnectionHandle connection_handle,
    l2cap::ChannelId l2cap_channel_id,
    uint16_t l2cap_mtu_size) {
  constexpr size_t kPacketSize =
      android_emb::StartA2dpOffloadCommand::MaxSizeInBytes();
  auto packet =
      hci::CommandPacket::New<android_emb::StartA2dpOffloadCommandWriter>(
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

DynamicByteBuffer UserConfirmationRequestPacket(DeviceAddress address,
                                                uint32_t passkey) {
  const auto addr = address.value().bytes();
  const auto passkey_bytes = ToBytes(passkey);
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kUserConfirmationRequestEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      passkey_bytes[0],  // Numeric_Value
      passkey_bytes[1],  // Numeric_Value
      passkey_bytes[2],  // Numeric_Value
      passkey_bytes[3]   // Numeric_Value
      ));
}

DynamicByteBuffer UserConfirmationRequestNegativeReplyPacket(
    DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kUserConfirmationRequestNegativeReply),
      UpperBits(hci_spec::kUserConfirmationRequestNegativeReply),
      0x06,  // parameter_total_size (6 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer UserConfirmationRequestReplyPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kUserConfirmationRequestReply),
                       UpperBits(hci_spec::kUserConfirmationRequestReply),
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer UserPasskeyNotificationPacket(DeviceAddress address,
                                                uint32_t passkey) {
  const auto addr = address.value().bytes();
  const auto passkey_bytes = ToBytes(passkey);
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kUserPasskeyNotificationEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      passkey_bytes[0],  // Numeric_Value
      passkey_bytes[1],  // Numeric_Value
      passkey_bytes[2],  // Numeric_Value
      passkey_bytes[3]   // Numeric_Value
      ));
}

DynamicByteBuffer UserPasskeyRequestNegativeReply(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kUserPasskeyRequestNegativeReply),
                       UpperBits(hci_spec::kUserPasskeyRequestNegativeReply),
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer UserPasskeyRequestNegativeReplyResponse(
    DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kUserPasskeyRequestNegativeReply),  // Command_Opcode
      UpperBits(hci_spec::kUserPasskeyRequestNegativeReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,             // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
}

DynamicByteBuffer UserPasskeyRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kUserPasskeyRequestEventCode,
                       0x06,  // parameter_total_size (6 bytes)
                       // peer BD_ADDR (6 bytes)
                       addr[0],
                       addr[1],
                       addr[2],
                       addr[3],
                       addr[4],
                       addr[5]));
}

DynamicByteBuffer UserPasskeyRequestReplyPacket(DeviceAddress address,
                                                uint32_t passkey) {
  const auto addr = address.value().bytes();
  const auto passkey_bytes = ToBytes(passkey);
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kUserPasskeyRequestReply),
      UpperBits(hci_spec::kUserPasskeyRequestReply),
      0x0A,  // parameter_total_size (10 bytes)
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],
      passkey_bytes[0],  // Numeric_Value
      passkey_bytes[1],  // Numeric_Value
      passkey_bytes[2],  // Numeric_Value
      passkey_bytes[3]   // Numeric_Value
      ));
}

DynamicByteBuffer UserPasskeyRequestReplyResponse(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandCompleteEventCode,
      0x0A,  // parameter_total_size (10 bytes)
      0xF0,  // Num_HCI_Command_Packets allowed to be sent to controller (240)
      LowerBits(hci_spec::kUserPasskeyRequestReply),  // Command_Opcode
      UpperBits(hci_spec::kUserPasskeyRequestReply),  // Command_Opcode
      pw::bluetooth::emboss::StatusCode::SUCCESS,     // Status
      // peer BD_ADDR (6 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]));
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

DynamicByteBuffer WritePageScanTypePacket(uint8_t scan_type) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePageScanType),
      UpperBits(hci_spec::kWritePageScanType),
      0x01,      // parameter_total_size (1 byte)
      scan_type  // Page_Scan_Type
      ));
}

DynamicByteBuffer WritePageScanTypeResponse() {
  return CommandCompletePacket(hci_spec::kWritePageScanType,
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

DynamicByteBuffer WritePinTypePacket(uint8_t pin_type) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePinType),
      UpperBits(hci_spec::kWritePinType),
      0x01,     // parameter_total_size (1 byte)
      pin_type  // PIN_Type
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

}  // namespace bt::testing
