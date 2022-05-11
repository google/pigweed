// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"

namespace bt::testing {

// This module contains functionality to create arbitrary HCI packets defining
// common behaviors with respect to expected devices and connections.
// This allows easily defining expected packets to be sent or received for
// given transactions such as connection establishment or discovery

DynamicByteBuffer CommandCompletePacket(hci_spec::OpCode opcode, hci_spec::StatusCode);

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address);

DynamicByteBuffer RejectConnectionRequestPacket(DeviceAddress address, hci_spec::StatusCode reason);

DynamicByteBuffer AuthenticationRequestedPacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer ConnectionRequestPacket(DeviceAddress address,
                                          hci_spec::LinkType link_type = hci_spec::LinkType::kACL);
DynamicByteBuffer CreateConnectionPacket(DeviceAddress address);
DynamicByteBuffer ConnectionCompletePacket(
    DeviceAddress address, hci_spec::ConnectionHandle conn,
    hci_spec::StatusCode status = hci_spec::StatusCode::kSuccess);

DynamicByteBuffer DisconnectPacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::StatusCode reason = hci_spec::StatusCode::kRemoteUserTerminatedConnection);
DynamicByteBuffer DisconnectStatusResponsePacket();
DynamicByteBuffer DisconnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::StatusCode reason = hci_spec::StatusCode::kRemoteUserTerminatedConnection);

DynamicByteBuffer EncryptionChangeEventPacket(hci_spec::StatusCode status_code,
                                              hci_spec::ConnectionHandle conn,
                                              hci_spec::EncryptionStatus encryption_enabled);

DynamicByteBuffer EnhancedAcceptSynchronousConnectionRequestPacket(
    DeviceAddress peer_address, hci_spec::SynchronousConnectionParameters params);

DynamicByteBuffer EnhancedSetupSynchronousConnectionPacket(
    hci_spec::ConnectionHandle conn, hci_spec::SynchronousConnectionParameters params);

DynamicByteBuffer NumberOfCompletedPacketsPacket(hci_spec::ConnectionHandle conn,
                                                 uint16_t num_packets);

DynamicByteBuffer CommandStatusPacket(hci_spec::OpCode op_code, hci_spec::StatusCode status_code);

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address);
DynamicByteBuffer RemoteNameRequestCompletePacket(DeviceAddress address,
                                                  const std::string& name = "Fuchsia💖");

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(hci_spec::ConnectionHandle conn,
                                                            bool extended_features);

DynamicByteBuffer RejectSynchronousConnectionRequest(DeviceAddress address,
                                                     hci_spec::StatusCode status_code);

DynamicByteBuffer RoleChangePacket(DeviceAddress address, hci_spec::ConnectionRole role,
                                   hci_spec::StatusCode status = hci_spec::StatusCode::kSuccess);

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn, bool enable);

DynamicByteBuffer SynchronousConnectionCompletePacket(hci_spec::ConnectionHandle conn,
                                                      DeviceAddress address,
                                                      hci_spec::LinkType link_type,
                                                      hci_spec::StatusCode status);

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(hci_spec::ConnectionHandle conn,
                                                     hci_spec::LESupportedFeatures le_features);

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle, uint64_t random_number,
                                          uint16_t encrypted_diversifier, UInt128 ltk);

// The ReadRemoteExtended*CompletePacket packets report a max page number of 3, even though there
// are only 2 pages, in order to test this behavior seen in real devices.
DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended1CompletePacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2CompletePacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(hci_spec::ConnectionHandle conn,
                                                   uint16_t flush_timeout);

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout);

DynamicByteBuffer ScoDataPacket(hci_spec::ConnectionHandle conn,
                                hci_spec::SynchronousDataPacketStatusFlag flag,
                                const BufferView& payload,
                                std::optional<uint8_t> payload_length_override = std::nullopt);

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
