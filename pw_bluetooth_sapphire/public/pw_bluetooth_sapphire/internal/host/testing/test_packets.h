// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"
#include "pw_bluetooth_sapphire/internal/host/transport/emboss_control_packets.h"

namespace bt::testing {

// This module contains functionality to create arbitrary HCI packets defining
// common behaviors with respect to expected devices and connections.
// This allows easily defining expected packets to be sent or received for
// given transactions such as connection establishment or discovery

DynamicByteBuffer EmptyCommandPacket(hci_spec::OpCode opcode);

DynamicByteBuffer CommandCompletePacket(hci_spec::OpCode opcode,
                                        pw::bluetooth::emboss::StatusCode);

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address);

DynamicByteBuffer RejectConnectionRequestPacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode reason);

DynamicByteBuffer AuthenticationRequestedPacket(
    hci_spec::ConnectionHandle conn);

DynamicByteBuffer ConnectionRequestPacket(
    DeviceAddress address,
    hci_spec::LinkType link_type = hci_spec::LinkType::kACL);
DynamicByteBuffer CreateConnectionPacket(DeviceAddress address);
DynamicByteBuffer ConnectionCompletePacket(
    DeviceAddress address,
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode status =
        pw::bluetooth::emboss::StatusCode::SUCCESS);

DynamicByteBuffer DisconnectPacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode reason =
        pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
DynamicByteBuffer DisconnectStatusResponsePacket();
DynamicByteBuffer DisconnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode reason =
        pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);

DynamicByteBuffer EncryptionChangeEventPacket(
    pw::bluetooth::emboss::StatusCode status_code,
    hci_spec::ConnectionHandle conn,
    hci_spec::EncryptionStatus encryption_enabled);

DynamicByteBuffer EnhancedAcceptSynchronousConnectionRequestPacket(
    DeviceAddress peer_address,
    bt::StaticPacket<
        pw::bluetooth::emboss::SynchronousConnectionParametersWriter> params);

DynamicByteBuffer EnhancedSetupSynchronousConnectionPacket(
    hci_spec::ConnectionHandle conn,
    bt::StaticPacket<
        pw::bluetooth::emboss::SynchronousConnectionParametersWriter> params);

DynamicByteBuffer NumberOfCompletedPacketsPacket(
    hci_spec::ConnectionHandle conn, uint16_t num_packets);

DynamicByteBuffer CommandStatusPacket(
    hci_spec::OpCode op_code, pw::bluetooth::emboss::StatusCode status_code);

DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address);
DynamicByteBuffer RemoteNameRequestCompletePacket(
    DeviceAddress address, const std::string& name = "Fuchsia💖");

DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(
    hci_spec::ConnectionHandle conn);

DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(
    hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, bool extended_features);

DynamicByteBuffer RejectSynchronousConnectionRequest(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code);

DynamicByteBuffer RoleChangePacket(
    DeviceAddress address,
    pw::bluetooth::emboss::ConnectionRole role,
    pw::bluetooth::emboss::StatusCode status =
        pw::bluetooth::emboss::StatusCode::SUCCESS);

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn,
                                          bool enable);

DynamicByteBuffer SynchronousConnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    DeviceAddress address,
    hci_spec::LinkType link_type,
    pw::bluetooth::emboss::StatusCode status);

DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn);
DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, hci_spec::LESupportedFeatures le_features);

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle,
                                          uint64_t random_number,
                                          uint16_t encrypted_diversifier,
                                          UInt128 ltk);

// The ReadRemoteExtended*CompletePacket packets report a max page number of 3,
// even though there are only 2 pages, in order to test this behavior seen in
// real devices.
DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended1CompletePacket(
    hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2CompletePacket(
    hci_spec::ConnectionHandle conn);

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(
    hci_spec::ConnectionHandle conn, uint16_t flush_timeout);

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout);

DynamicByteBuffer ScoDataPacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::SynchronousDataPacketStatusFlag flag,
    const BufferView& payload,
    std::optional<uint8_t> payload_length_override = std::nullopt);

DynamicByteBuffer StartA2dpOffloadRequest(
    const l2cap::A2dpOffloadManager::Configuration& config,
    hci_spec::ConnectionHandle connection_handle,
    l2cap::ChannelId l2cap_channel_id,
    uint16_t l2cap_mtu_size);

DynamicByteBuffer StopA2dpOffloadRequest();

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
