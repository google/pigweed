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

DynamicByteBuffer CommandCompletePacket(hci::OpCode opcode, hci::StatusCode);

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address);

DynamicByteBuffer RejectConnectionRequestPacket(DeviceAddress address, hci::StatusCode reason);

DynamicByteBuffer AuthenticationRequestedPacket(hci::ConnectionHandle conn);

DynamicByteBuffer ConnectionRequestPacket(DeviceAddress address,
                                          hci::LinkType link_type = hci::LinkType::kACL);
DynamicByteBuffer CreateConnectionPacket(DeviceAddress address);
DynamicByteBuffer ConnectionCompletePacket(DeviceAddress address, hci::ConnectionHandle conn,
                                           hci::StatusCode status = hci::StatusCode::kSuccess);

DynamicByteBuffer DisconnectPacket(
    hci::ConnectionHandle conn,
    hci::StatusCode reason = hci::StatusCode::kRemoteUserTerminatedConnection);
DynamicByteBuffer DisconnectStatusResponsePacket();
DynamicByteBuffer DisconnectionCompletePacket(
    hci::ConnectionHandle conn,
    hci::StatusCode reason = hci::StatusCode::kRemoteUserTerminatedConnection);

DynamicByteBuffer EncryptionChangeEventPacket(hci::StatusCode status_code,
                                              hci::ConnectionHandle conn,
                                              hci::EncryptionStatus encryption_enabled);

DynamicByteBuffer EnhancedAcceptSynchronousConnectionRequestPacket(
    DeviceAddress peer_address, hci::SynchronousConnectionParameters params);

DynamicByteBuffer EnhancedSetupSynchronousConnectionPacket(
    hci::ConnectionHandle conn, hci::SynchronousConnectionParameters params);

DynamicByteBuffer NumberOfCompletedPacketsPacket(hci::ConnectionHandle conn, uint16_t num_packets);

DynamicByteBuffer CommandStatusPacket(hci::OpCode op_code, hci::StatusCode status_code);

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address);
DynamicByteBuffer RemoteNameRequestCompletePacket(DeviceAddress address,
                                                  const std::string& name = u8"Fuchsia💖");

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(hci::ConnectionHandle conn);

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(hci::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(hci::ConnectionHandle conn,
                                                            bool extended_features);

DynamicByteBuffer RejectSynchronousConnectionRequest(DeviceAddress address,
                                                     hci::StatusCode status_code);

DynamicByteBuffer RoleChangePacket(DeviceAddress address, hci::ConnectionRole role,
                                   hci::StatusCode status = hci::StatusCode::kSuccess);

DynamicByteBuffer SetConnectionEncryption(hci::ConnectionHandle conn, bool enable);

DynamicByteBuffer SynchronousConnectionCompletePacket(hci::ConnectionHandle conn,
                                                      DeviceAddress address,
                                                      hci::LinkType link_type,
                                                      hci::StatusCode status);

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci::ConnectionHandle conn);
DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(hci::ConnectionHandle conn,
                                                     hci::LESupportedFeatures le_features);

DynamicByteBuffer LEStartEncryptionPacket(hci::ConnectionHandle, uint64_t random_number,
                                          uint16_t encrypted_diversifier, UInt128 ltk);

// The ReadRemoteExtended*CompletePacket packets report a max page number of 3, even though there
// are only 2 pages, in order to test this behavior seen in real devices.
DynamicByteBuffer ReadRemoteExtended1Packet(hci::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended1CompletePacket(hci::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2Packet(hci::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2CompletePacket(hci::ConnectionHandle conn);

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(hci::ConnectionHandle conn,
                                                   uint16_t flush_timeout);

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
