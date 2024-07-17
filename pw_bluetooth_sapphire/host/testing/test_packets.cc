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

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/bredr_connection_request.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::testing {

namespace android_hci = bt::hci_spec::vendor::android;
namespace android_emb = pw::bluetooth::vendor::android_hci;

// clang-format off
#define COMMAND_STATUS_RSP(opcode, statuscode)                       \
StaticByteBuffer( hci_spec::kCommandStatusEventCode, 0x04,         \
                                (statuscode), 0xF0,                 \
                                LowerBits((opcode)), UpperBits((opcode)))

#define UINT32_TO_LE(bits)                      \
  static_cast<uint32_t>(bits),                  \
  static_cast<uint32_t>(bits) >> CHAR_BIT,      \
  static_cast<uint32_t>(bits) >> 2 * CHAR_BIT,  \
  static_cast<uint32_t>(bits) >> 3 * CHAR_BIT
// clang-format on

DynamicByteBuffer EmptyCommandPacket(hci_spec::OpCode opcode) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(opcode), UpperBits(opcode), /*length=*/0));
}

DynamicByteBuffer CommandCompletePacket(
    hci_spec::OpCode opcode, pw::bluetooth::emboss::StatusCode status) {
  return DynamicByteBuffer(StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                                            0x04,  // size
                                            0x01,  // Num HCI command packets
                                            LowerBits(opcode),
                                            UpperBits(opcode),  // Op code
                                            status));
}

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kAcceptConnectionRequest),
      UpperBits(hci_spec::kAcceptConnectionRequest),
      0x07,  // parameter_total_size (7 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],  // peer address
      0x00      // role (become central)
      ));
}

DynamicByteBuffer RejectConnectionRequestPacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode reason) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRejectConnectionRequest),
      UpperBits(hci_spec::kRejectConnectionRequest),
      0x07,  // parameter_total_size (7 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],  // peer address
      reason    // reason
      ));
}

DynamicByteBuffer AuthenticationRequestedPacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kAuthenticationRequested),
      UpperBits(hci_spec::kAuthenticationRequested),
      0x02,  // parameter_total_size (2 bytes)
      LowerBits(conn),
      UpperBits(conn)  // Connection_Handle
      ));
}

DynamicByteBuffer ConnectionRequestPacket(DeviceAddress address,
                                          hci_spec::LinkType link_type) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kConnectionRequestEventCode,
      0x0A,  // parameter_total_size (10 byte payload)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],  // peer address
      0x00,
      0x1F,
      0x00,      // class_of_device (unspecified)
      link_type  // link_type
      ));
}

DynamicByteBuffer CreateConnectionPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kCreateConnection),
      UpperBits(hci_spec::kCreateConnection),
      0x0d,  // parameter_total_size (13 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],                                // peer address
      LowerBits(hci::kEnableAllPacketTypes),  // allowable packet types
      UpperBits(hci::kEnableAllPacketTypes),  // allowable packet types
      0x02,                                   // page_scan_repetition_mode (R2)
      0x00,                                   // reserved
      0x00,
      0x00,  // clock_offset
      0x00   // allow_role_switch (don't)
      ));
}

DynamicByteBuffer ConnectionCompletePacket(
    DeviceAddress address,
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode status) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kConnectionCompleteEventCode,
      0x0B,    // parameter_total_size (11 byte payload)
      status,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],  // peer address
      0x01,     // link_type (ACL)
      0x00      // encryption not enabled
      ));
}

DynamicByteBuffer DisconnectPacket(hci_spec::ConnectionHandle conn,
                                   pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kDisconnect),
      UpperBits(hci_spec::kDisconnect),
      0x03,  // parameter_total_size (3 bytes)
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      reason            // Reason
      ));
}

DynamicByteBuffer DisconnectStatusResponsePacket() {
  return DynamicByteBuffer(COMMAND_STATUS_RSP(
      hci_spec::kDisconnect, pw::bluetooth::emboss::StatusCode::SUCCESS));
}

DynamicByteBuffer DisconnectionCompletePacket(
    hci_spec::ConnectionHandle conn, pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kDisconnectionCompleteEventCode,
      0x04,  // parameter_total_size (4 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      reason            // Reason
      ));
}

DynamicByteBuffer EncryptionChangeEventPacket(
    pw::bluetooth::emboss::StatusCode status_code,
    hci_spec::ConnectionHandle conn,
    hci_spec::EncryptionStatus encryption_enabled) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kEncryptionChangeEventCode,
      0x04,         // parameter_total_size (4 bytes)
      status_code,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_Handle
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

DynamicByteBuffer NumberOfCompletedPacketsPacket(
    hci_spec::ConnectionHandle conn, uint16_t num_packets) {
  return DynamicByteBuffer(StaticByteBuffer(
      0x13,
      0x05,  // Number Of Completed Packet HCI event header, parameters length
      0x01,  // Number of handles
      LowerBits(conn),
      UpperBits(conn),
      LowerBits(num_packets),
      UpperBits(num_packets)));
}

DynamicByteBuffer CommandStatusPacket(
    hci_spec::OpCode op_code, pw::bluetooth::emboss::StatusCode status_code) {
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kCommandStatusEventCode,
                       0x04,  // parameter size (4 bytes)
                       status_code,
                       0xF0,  // number of HCI command packets allowed to be
                              // sent to controller (240)
                       LowerBits(op_code),
                       UpperBits(op_code)));
}

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRemoteNameRequest),
      UpperBits(hci_spec::kRemoteNameRequest),
      0x0a,  // parameter_total_size (10 bytes)
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5],  // peer address
      0x00,     // page_scan_repetition_mode (R0)
      0x00,     // reserved
      0x00,
      0x00  // clock_offset
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
      0xff,                                        // parameter_total_size (255)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      addr[0],
      addr[1],
      addr[2],
      addr[3],
      addr[4],
      addr[5]  // peer address
  );
  header.Copy(&event);
  event.Write(reinterpret_cast<const uint8_t*>(name.data()),
              name.size(),
              header.size());
  return event;
}

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteVersionInfo),
      UpperBits(hci_spec::kReadRemoteVersionInfo),
      0x02,  // Parameter_total_size (2 bytes)
      LowerBits(conn),
      UpperBits(conn)  // Little-Endian Connection_handle
      ));
}

DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteVersionInfoCompleteEventCode,
      0x08,  // parameter_total_size (8 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      pw::bluetooth::emboss::CoreSpecificationVersion::V4_2,  // version
      0xE0,
      0x00,  // company_identifier (Google)
      0xAD,
      0xDE  // subversion (anything)
      ));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kReadRemoteSupportedFeatures),
                       UpperBits(hci_spec::kReadRemoteSupportedFeatures),
                       0x02,             // parameter_total_size (2 bytes)
                       LowerBits(conn),  // Little-Endian Connection_handle
                       UpperBits(conn)));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, bool extended_features) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
      0x0B,  // parameter_total_size (11 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      0xFF,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      (extended_features ? 0x80 : 0x00)
      // lmp_features
      // Set: 3 slot packets, 5 slot packets, Encryption,
      // Timing Accuracy, Role Switch, Hold Mode, Sniff Mode,
      // LE Supported Extended Features if enabled
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
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                       0x05,  // Size
                       0xF0,  // Number HCI command packets
                       LowerBits(hci_spec::kReadScanEnable),
                       UpperBits(hci_spec::kReadScanEnable),
                       pw::bluetooth::emboss::StatusCode::SUCCESS,
                       scan_enable));
}

DynamicByteBuffer RejectSynchronousConnectionRequest(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRejectSynchronousConnectionRequest),
      UpperBits(hci_spec::kRejectSynchronousConnectionRequest),
      0x07,  // parameter total size
      addr_bytes[0],
      addr_bytes[1],
      addr_bytes[2],
      addr_bytes[3],
      addr_bytes[4],
      addr_bytes[5],  // peer address
      status_code     // reason
      ));
}

DynamicByteBuffer RoleChangePacket(DeviceAddress address,
                                   pw::bluetooth::emboss::ConnectionRole role,
                                   pw::bluetooth::emboss::StatusCode status) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(hci_spec::kRoleChangeEventCode,
                                            0x08,    // parameter_total_size
                                            status,  // status
                                            addr_bytes[0],
                                            addr_bytes[1],
                                            addr_bytes[2],
                                            addr_bytes[3],
                                            addr_bytes[4],
                                            addr_bytes[5],  // peer address
                                            role));
}

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn,
                                          bool enable) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kSetConnectionEncryption),
                       UpperBits(hci_spec::kSetConnectionEncryption),
                       0x03,  // parameter total size (3 bytes)
                       LowerBits(conn),
                       UpperBits(conn),
                       static_cast<uint8_t>(enable)));
}

DynamicByteBuffer SynchronousConnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    DeviceAddress address,
    hci_spec::LinkType link_type,
    pw::bluetooth::emboss::StatusCode status) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kSynchronousConnectionCompleteEventCode,
      0x11,  // parameter_total_size (17 bytes)
      status,
      LowerBits(conn),
      UpperBits(conn),
      addr_bytes[0],
      addr_bytes[1],
      addr_bytes[2],
      addr_bytes[3],
      addr_bytes[4],
      addr_bytes[5],
      link_type,  // peer address
      0x00,       // transmission interval
      0x00,       // retransmission window
      0x00,
      0x00,  // rx packet length
      0x00,
      0x00,  // tx packet length
      0x00   // coding format
      ));
}

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLEReadRemoteFeatures),
                       UpperBits(hci_spec::kLEReadRemoteFeatures),
                       0x02,             // parameter_total_size (2 bytes)
                       LowerBits(conn),  // Little-Endian Connection_handle
                       UpperBits(conn)));
}

DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::LESupportedFeatures le_features) {
  const BufferView features(&le_features, sizeof(le_features));
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kLEMetaEventCode,
                       0x0c,  // parameter total size (12 bytes)
                       hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
                       pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
                       // Little-Endian connection handle
                       LowerBits(conn),
                       UpperBits(conn),
                       // bit mask of LE features
                       features[0],
                       features[1],
                       features[2],
                       features[3],
                       features[4],
                       features[5],
                       features[6],
                       features[7]));
}

DynamicByteBuffer LECISRequestEventPacket(
    hci_spec::ConnectionHandle acl_connection_handle,
    hci_spec::ConnectionHandle cis_connection_handle,
    hci_spec::CigIdentifier cig_id,
    hci_spec::CisIdentifier cis_id) {
  return DynamicByteBuffer(
      StaticByteBuffer(hci_spec::kLEMetaEventCode,
                       0x07,  // parameter total size (7 bytes)
                       hci_spec::kLECISRequestSubeventCode,
                       LowerBits(acl_connection_handle),
                       UpperBits(acl_connection_handle),
                       LowerBits(cis_connection_handle),
                       UpperBits(cis_connection_handle),
                       cig_id,
                       cis_id));
}

DynamicByteBuffer LERejectCISRequestCommandPacket(
    hci_spec::ConnectionHandle cis_handle,
    pw::bluetooth::emboss::StatusCode reason) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLERejectCISRequest),
                       UpperBits(hci_spec::kLERejectCISRequest),
                       0x03,  // parameter total size (3 bytes)
                       LowerBits(cis_handle),
                       UpperBits(cis_handle),
                       reason));
}

DynamicByteBuffer LERequestPeerScaPacket(hci_spec::ConnectionHandle conn) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LERequestPeerSCACommandWriter>(
      hci_spec::kLERequestPeerSCA);
  auto view = packet.view_t();
  view.connection_handle().Write(conn);
  return DynamicByteBuffer(packet.data());
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

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle conn,
                                          uint64_t random_number,
                                          uint16_t encrypted_diversifier,
                                          UInt128 ltk) {
  const BufferView rand(&random_number, sizeof(random_number));
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kLEStartEncryption),
                       UpperBits(hci_spec::kLEStartEncryption),
                       0x1c,  // parameter total size (28 bytes)
                       LowerBits(conn),
                       UpperBits(conn),  // Connection_handle
                       rand[0],
                       rand[1],
                       rand[2],
                       rand[3],
                       rand[4],
                       rand[5],
                       rand[6],
                       rand[7],
                       LowerBits(encrypted_diversifier),
                       UpperBits(encrypted_diversifier),
                       // LTK
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

DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,             // parameter_total_size (3 bytes)
      LowerBits(conn),  // Little-Endian Connection_handle
      UpperBits(conn),
      0x01  // page_number (1)
      ));
}

DynamicByteBuffer ReadRemoteExtended1CompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,  // parameter_total_size (13 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      0x01,             // page_number
      0x03,             // max_page_number (3 pages)
      0x0F,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00
      // lmp_features (page 1)
      // Set: Secure Simple Pairing (Host Support), LE Supported (Host),
      // SimultaneousLEAndBREDR, Secure Connections (Host Support)
      ));
}

DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,  // parameter_total_size (3 bytes)
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      0x02              // page_number (2)
      ));
}

DynamicByteBuffer ReadRemoteExtended2CompletePacket(
    hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,  // parameter_total_size (13 bytes)
      pw::bluetooth::emboss::StatusCode::SUCCESS,  // status
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_handle
      0x02,             // page_number
      0x03,             // max_page_number (3 pages)
      0x00,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0xFF,
      0x00
      // lmp_features  - All the bits should be ignored.
      ));
}

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(
    hci_spec::ConnectionHandle conn, uint16_t flush_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWriteAutomaticFlushTimeout),
      UpperBits(hci_spec::kWriteAutomaticFlushTimeout),
      0x04,  // parameter_total_size (4 bytes)
      LowerBits(conn),
      UpperBits(conn),  // Little-Endian Connection_Handle
      LowerBits(flush_timeout),
      UpperBits(flush_timeout)  // Little-Endian Flush_Timeout
      ));
}

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePageTimeout),
      UpperBits(hci_spec::kWritePageTimeout),
      0x02,  // parameter_total_size (2 bytes)
      LowerBits(page_timeout),
      UpperBits(page_timeout)  // Little-Endian Page_Timeout
      ));
}

DynamicByteBuffer WriteScanEnable(uint8_t scan_enable) {
  return DynamicByteBuffer(
      StaticByteBuffer(LowerBits(hci_spec::kWriteScanEnable),
                       UpperBits(hci_spec::kWriteScanEnable),
                       0x01,
                       scan_enable));
}

DynamicByteBuffer WriteScanEnableResponse() {
  return CommandCompletePacket(hci_spec::kWriteScanEnable,
                               pw::bluetooth::emboss::StatusCode::SUCCESS);
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
  return DynamicByteBuffer(StaticByteBuffer(
      // clang-format off
      LowerBits(android_hci::kA2dpOffloadCommand), UpperBits(android_hci::kA2dpOffloadCommand),
      0x01,  // parameter_total_size (1 byte)
      android_hci::kStopA2dpOffloadCommandSubopcode));
  // clang-format on
}

}  // namespace bt::testing
