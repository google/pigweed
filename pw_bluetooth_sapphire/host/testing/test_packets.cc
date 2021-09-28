// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_packets.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection_request.h"

namespace bt::testing {

namespace {

hci_spec::SynchronousConnectionParameters ConnectionParametersToLe(
    hci_spec::SynchronousConnectionParameters params) {
  params.transmit_bandwidth = htole32(params.transmit_bandwidth);
  params.receive_bandwidth = htole32(params.receive_bandwidth);
  params.transmit_coding_format.company_id = htole16(params.transmit_coding_format.company_id);
  params.transmit_coding_format.vendor_codec_id =
      htole16(params.transmit_coding_format.vendor_codec_id);
  params.receive_coding_format.company_id = htole16(params.receive_coding_format.company_id);
  params.receive_coding_format.vendor_codec_id =
      htole16(params.receive_coding_format.vendor_codec_id);
  params.transmit_codec_frame_size_bytes = htole16(params.transmit_codec_frame_size_bytes);
  params.receive_codec_frame_size_bytes = htole16(params.receive_codec_frame_size_bytes);
  params.input_bandwidth = htole32(params.input_bandwidth);
  params.output_bandwidth = htole32(params.output_bandwidth);
  params.input_coding_format.company_id = htole16(params.input_coding_format.company_id);
  params.input_coding_format.vendor_codec_id = htole16(params.input_coding_format.vendor_codec_id);
  params.output_coding_format.company_id = htole16(params.output_coding_format.company_id);
  params.output_coding_format.vendor_codec_id =
      htole16(params.output_coding_format.vendor_codec_id);
  params.max_latency_ms = htole16(params.max_latency_ms);
  params.packet_types = htole16(params.packet_types);
  return params;
}

}  // namespace

// clang-format off
#define COMMAND_STATUS_RSP(opcode, statuscode)                       \
CreateStaticByteBuffer( hci_spec::kCommandStatusEventCode, 0x04,         \
                                (statuscode), 0xF0,                 \
                                LowerBits((opcode)), UpperBits((opcode)))
// clang-format on

DynamicByteBuffer CommandCompletePacket(hci_spec::OpCode opcode, hci_spec::StatusCode status) {
  return DynamicByteBuffer(StaticByteBuffer(hci_spec::kCommandCompleteEventCode,
                                            0x04,  // size
                                            0x01,  // Num HCI command packets
                                            LowerBits(opcode), UpperBits(opcode),  // Op code
                                            status));
}

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kAcceptConnectionRequest), UpperBits(hci_spec::kAcceptConnectionRequest),
      0x07,                                                  // parameter_total_size (7 bytes)
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      0x00                                                   // role (become master)
      ));
}

DynamicByteBuffer RejectConnectionRequestPacket(DeviceAddress address,
                                                hci_spec::StatusCode reason) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kRejectConnectionRequest), UpperBits(hci_spec::kRejectConnectionRequest),
      0x07,                                                  // parameter_total_size (7 bytes)
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      reason                                                 // reason
      ));
}

DynamicByteBuffer AuthenticationRequestedPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kAuthenticationRequested), UpperBits(hci_spec::kAuthenticationRequested),
      0x02,                             // parameter_total_size (2 bytes)
      LowerBits(conn), UpperBits(conn)  // Connection_Handle
      ));
}

DynamicByteBuffer ConnectionRequestPacket(DeviceAddress address, hci_spec::LinkType link_type) {
  const auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kConnectionRequestEventCode,
      0x0A,  // parameter_total_size (10 byte payload)
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      0x00, 0x1F, 0x00,                                      // class_of_device (unspecified)
      link_type                                              // link_type
      ));
}

DynamicByteBuffer CreateConnectionPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kCreateConnection), UpperBits(hci_spec::kCreateConnection),
      0x0d,                                                  // parameter_total_size (13 bytes)
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      LowerBits(hci::kEnableAllPacketTypes),                 // allowable packet types
      UpperBits(hci::kEnableAllPacketTypes),                 // allowable packet types
      0x02,                                                  // page_scan_repetition_mode (R2)
      0x00,                                                  // reserved
      0x00, 0x00,                                            // clock_offset
      0x00                                                   // allow_role_switch (don't)
      ));
}

DynamicByteBuffer ConnectionCompletePacket(DeviceAddress address, hci_spec::ConnectionHandle conn,
                                           hci_spec::StatusCode status) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kConnectionCompleteEventCode,
      0x0B,                              // parameter_total_size (11 byte payload)
      status,                            // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      0x01,                                                  // link_type (ACL)
      0x00                                                   // encryption not enabled
      ));
}

DynamicByteBuffer DisconnectPacket(hci_spec::ConnectionHandle conn, hci_spec::StatusCode reason) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kDisconnect), UpperBits(hci_spec::kDisconnect),
      0x03,                              // parameter_total_size (3 bytes)
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      reason                             // Reason
      ));
}

DynamicByteBuffer DisconnectStatusResponsePacket() {
  return DynamicByteBuffer(
      COMMAND_STATUS_RSP(hci_spec::kDisconnect, hci_spec::StatusCode::kSuccess));
}

DynamicByteBuffer DisconnectionCompletePacket(hci_spec::ConnectionHandle conn,
                                              hci_spec::StatusCode reason) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kDisconnectionCompleteEventCode,
      0x04,                              // parameter_total_size (4 bytes)
      hci_spec::StatusCode::kSuccess,    // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      reason                             // Reason
      ));
}

DynamicByteBuffer EncryptionChangeEventPacket(hci_spec::StatusCode status_code,
                                              hci_spec::ConnectionHandle conn,
                                              hci_spec::EncryptionStatus encryption_enabled) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kEncryptionChangeEventCode,
      0x04,                                     // parameter_total_size (4 bytes)
      status_code,                              // status
      LowerBits(conn), UpperBits(conn),         // Little-Endian Connection_Handle
      static_cast<uint8_t>(encryption_enabled)  // Encryption_Enabled
      ));
}

DynamicByteBuffer EnhancedAcceptSynchronousConnectionRequestPacket(
    DeviceAddress peer_address, hci_spec::SynchronousConnectionParameters params) {
  StaticByteBuffer<sizeof(hci_spec::CommandHeader) +
                   sizeof(hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandParams)>
      buffer;
  auto& header = buffer.AsMutable<hci_spec::CommandHeader>();
  header.opcode = htole16(hci_spec::kEnhancedAcceptSynchronousConnectionRequest);
  header.parameter_total_size =
      sizeof(hci_spec::EnhancedAcceptSynchronousConnectionRequestCommandParams);

  buffer.mutable_view(sizeof(hci_spec::CommandHeader)).AsMutable<DeviceAddressBytes>() =
      peer_address.value();

  auto& payload = buffer.mutable_view(sizeof(hci_spec::CommandHeader) + sizeof(DeviceAddressBytes))
                      .AsMutable<hci_spec::SynchronousConnectionParameters>();
  payload = ConnectionParametersToLe(params);
  return DynamicByteBuffer(buffer);
}

DynamicByteBuffer EnhancedSetupSynchronousConnectionPacket(
    hci_spec::ConnectionHandle conn, hci_spec::SynchronousConnectionParameters params) {
  StaticByteBuffer<sizeof(hci_spec::CommandHeader) +
                   sizeof(hci_spec::EnhancedSetupSynchronousConnectionCommandParams)>
      buffer;
  auto& header = buffer.AsMutable<hci_spec::CommandHeader>();
  header.opcode = htole16(hci_spec::kEnhancedSetupSynchronousConnection);
  header.parameter_total_size = sizeof(hci_spec::EnhancedSetupSynchronousConnectionCommandParams);

  buffer.mutable_view(sizeof(hci_spec::CommandHeader)).AsMutable<hci_spec::ConnectionHandle>() =
      htole16(conn);

  auto& payload =
      buffer.mutable_view(sizeof(hci_spec::CommandHeader) + sizeof(hci_spec::ConnectionHandle))
          .AsMutable<hci_spec::SynchronousConnectionParameters>();
  payload = ConnectionParametersToLe(params);
  return DynamicByteBuffer(buffer);
}

DynamicByteBuffer NumberOfCompletedPacketsPacket(hci_spec::ConnectionHandle conn,
                                                 uint16_t num_packets) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      0x13, 0x05,  // Number Of Completed Packet HCI event header, parameters length
      0x01,        // Number of handles
      LowerBits(conn), UpperBits(conn), LowerBits(num_packets), UpperBits(num_packets)));
}

DynamicByteBuffer CommandStatusPacket(hci_spec::OpCode op_code, hci_spec::StatusCode status_code) {
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kCommandStatusEventCode,
      0x04,  // parameter size (4 bytes)
      status_code,
      0xF0,  // number of HCI command packets allowed to be sent to controller (240)
      LowerBits(op_code), UpperBits(op_code)));
}

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address) {
  auto addr = address.value().bytes();
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kRemoteNameRequest), UpperBits(hci_spec::kRemoteNameRequest),
      0x0a,                                                  // parameter_total_size (10 bytes)
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],  // peer address
      0x00,                                                  // page_scan_repetition_mode (R0)
      0x00,                                                  // reserved
      0x00, 0x00                                             // clock_offset
      ));
}

DynamicByteBuffer RemoteNameRequestCompletePacket(DeviceAddress address, const std::string& name) {
  auto addr = address.value().bytes();
  auto event = DynamicByteBuffer(sizeof(hci_spec::EventHeader) +
                                 sizeof(hci_spec::RemoteNameRequestCompleteEventParams));
  event.SetToZeros();
  const StaticByteBuffer header(hci_spec::kRemoteNameRequestCompleteEventCode,
                                0xff,                            // parameter_total_size (255)
                                hci_spec::StatusCode::kSuccess,  // status
                                addr[0], addr[1], addr[2], addr[3], addr[4],
                                addr[5]  // peer address
  );
  header.Copy(&event);
  event.Write(reinterpret_cast<const uint8_t*>(name.data()), name.size(), header.size());
  return event;
}

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteVersionInfo), UpperBits(hci_spec::kReadRemoteVersionInfo),
      0x02,                             // Parameter_total_size (2 bytes)
      LowerBits(conn), UpperBits(conn)  // Little-Endian Connection_handle
      ));
}

DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kReadRemoteVersionInfoCompleteEventCode,
      0x08,                              // parameter_total_size (8 bytes)
      hci_spec::StatusCode::kSuccess,    // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      hci_spec::HCIVersion::k4_2,        // lmp_version
      0xE0, 0x00,                        // manufacturer_name (Google)
      0xAD, 0xDE                         // lmp_subversion (anything)
      ));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(
      CreateStaticByteBuffer(LowerBits(hci_spec::kReadRemoteSupportedFeatures),
                             UpperBits(hci_spec::kReadRemoteSupportedFeatures),
                             0x02,             // parameter_total_size (2 bytes)
                             LowerBits(conn),  // Little-Endian Connection_handle
                             UpperBits(conn)));
}

DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(hci_spec::ConnectionHandle conn,
                                                            bool extended_features) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode,
      0x0B,                              // parameter_total_size (11 bytes)
      hci_spec::StatusCode::kSuccess,    // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, (extended_features ? 0x80 : 0x00)
      // lmp_features
      // Set: 3 slot packets, 5 slot packets, Encryption, Timing Accuracy,
      // Role Switch, Hold Mode, Sniff Mode, LE Supported
      // Extended Features if enabled
      ));
}

DynamicByteBuffer RejectSynchronousConnectionRequest(DeviceAddress address,
                                                     hci_spec::StatusCode status_code) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kRejectSynchronousConnectionRequest),
      UpperBits(hci_spec::kRejectSynchronousConnectionRequest),
      0x07,  // parameter total size
      addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3], addr_bytes[4],
      addr_bytes[5],  // peer address
      status_code     // reason
      ));
}

DynamicByteBuffer RoleChangePacket(DeviceAddress address, hci_spec::ConnectionRole role,
                                   hci_spec::StatusCode status) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(hci_spec::kRoleChangeEventCode,
                                            0x08,    // parameter_total_size
                                            status,  // status
                                            addr_bytes[0], addr_bytes[1], addr_bytes[2],
                                            addr_bytes[3], addr_bytes[4],
                                            addr_bytes[5],  // peer address
                                            role));
}

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn, bool enable) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kSetConnectionEncryption), UpperBits(hci_spec::kSetConnectionEncryption),
      0x03,  // parameter total size (3 bytes)
      LowerBits(conn), UpperBits(conn), static_cast<uint8_t>(enable)));
}

DynamicByteBuffer SynchronousConnectionCompletePacket(hci_spec::ConnectionHandle conn,
                                                      DeviceAddress address,
                                                      hci_spec::LinkType link_type,
                                                      hci_spec::StatusCode status) {
  auto addr_bytes = address.value().bytes();
  return DynamicByteBuffer(StaticByteBuffer(
      hci_spec::kSynchronousConnectionCompleteEventCode,
      0x11,  // parameter_total_size (17 bytes)
      status, LowerBits(conn), UpperBits(conn), addr_bytes[0], addr_bytes[1], addr_bytes[2],
      addr_bytes[3], addr_bytes[4], addr_bytes[5], link_type,  // peer address
      0x00,                                                    // transmission interval
      0x00,                                                    // retransmission window
      0x00, 0x00,                                              // rx packet length
      0x00, 0x00,                                              // tx packet length
      0x00                                                     // coding format
      ));
}

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kLEReadRemoteFeatures), UpperBits(hci_spec::kLEReadRemoteFeatures),
      0x02,             // parameter_total_size (2 bytes)
      LowerBits(conn),  // Little-Endian Connection_handle
      UpperBits(conn)));
}

DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(hci_spec::ConnectionHandle conn,
                                                     hci_spec::LESupportedFeatures le_features) {
  const BufferView features(&le_features, sizeof(le_features));
  return DynamicByteBuffer(StaticByteBuffer(hci_spec::kLEMetaEventCode,
                                            0x0c,  // parameter total size (12 bytes)
                                            hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
                                            hci_spec::StatusCode::kSuccess,  // status
                                            // Little-Endian connection handle
                                            LowerBits(conn), UpperBits(conn),
                                            // bit mask of LE features
                                            features[0], features[1], features[2], features[3],
                                            features[4], features[5], features[6], features[7]));
}

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle conn, uint64_t random_number,
                                          uint16_t encrypted_diversifier, UInt128 ltk) {
  const BufferView rand(&random_number, sizeof(random_number));
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kLEStartEncryption), UpperBits(hci_spec::kLEStartEncryption),
      0x1c,                              // parameter total size (28 bytes)
      LowerBits(conn), UpperBits(conn),  // Connection_handle
      rand[0], rand[1], rand[2], rand[3], rand[4], rand[5], rand[6], rand[7],
      LowerBits(encrypted_diversifier), UpperBits(encrypted_diversifier),
      // LTK
      ltk[0], ltk[1], ltk[2], ltk[3], ltk[4], ltk[5], ltk[6], ltk[7], ltk[8], ltk[9], ltk[10],
      ltk[11], ltk[12], ltk[13], ltk[14], ltk[15]));
}

DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,             // parameter_total_size (3 bytes)
      LowerBits(conn),  // Little-Endian Connection_handle
      UpperBits(conn),
      0x01  // page_number (1)
      ));
}

DynamicByteBuffer ReadRemoteExtended1CompletePacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,                              // parameter_total_size (13 bytes)
      hci_spec::StatusCode::kSuccess,    // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      0x01,                              // page_number
      0x03,                              // max_page_number (3 pages)
      0x0F, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00
      // lmp_features (page 1)
      // Set: Secure Simple Pairing (Host Support), LE Supported (Host),
      //  SimultaneousLEAndBREDR, Secure Connections (Host Support)
      ));
}

DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      LowerBits(hci_spec::kReadRemoteExtendedFeatures),
      UpperBits(hci_spec::kReadRemoteExtendedFeatures),
      0x03,                              // parameter_total_size (3 bytes)
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      0x02                               // page_number (2)
      ));
}

DynamicByteBuffer ReadRemoteExtended2CompletePacket(hci_spec::ConnectionHandle conn) {
  return DynamicByteBuffer(CreateStaticByteBuffer(
      hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode,
      0x0D,                              // parameter_total_size (13 bytes)
      hci_spec::StatusCode::kSuccess,    // status
      LowerBits(conn), UpperBits(conn),  // Little-Endian Connection_handle
      0x02,                              // page_number
      0x03,                              // max_page_number (3 pages)
      0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0xFF, 0x00
      // lmp_features  - All the bits should be ignored.
      ));
}

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(hci_spec::ConnectionHandle conn,
                                                   uint16_t flush_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWriteAutomaticFlushTimeout),
      UpperBits(hci_spec::kWriteAutomaticFlushTimeout),
      0x04,                                               // parameter_total_size (4 bytes)
      LowerBits(conn), UpperBits(conn),                   // Little-Endian Connection_Handle
      LowerBits(flush_timeout), UpperBits(flush_timeout)  // Little-Endian Flush_Timeout
      ));
}

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout) {
  return DynamicByteBuffer(StaticByteBuffer(
      LowerBits(hci_spec::kWritePageTimeout), UpperBits(hci_spec::kWritePageTimeout),
      0x02,                                             // parameter_total_size (2 bytes)
      LowerBits(page_timeout), UpperBits(page_timeout)  // Little-Endian Page_Timeout
      ));
}

}  // namespace bt::testing
