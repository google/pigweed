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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/a2dp_offload_manager.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/transport/emboss_packet.h"

namespace bt::testing {

// This module contains functionality to create arbitrary HCI packets defining
// common behaviors with respect to expected devices and connections.
// This allows easily defining expected packets to be sent or received for
// given transactions such as connection establishment or discovery

DynamicByteBuffer AcceptConnectionRequestPacket(DeviceAddress address);

DynamicByteBuffer AuthenticationRequestedPacket(
    hci_spec::ConnectionHandle conn);

DynamicByteBuffer CommandCompletePacket(hci_spec::OpCode opcode,
                                        pw::bluetooth::emboss::StatusCode);

DynamicByteBuffer CommandStatusPacket(
    hci_spec::OpCode op_code, pw::bluetooth::emboss::StatusCode status_code);

DynamicByteBuffer ConnectionCompletePacket(
    DeviceAddress address,
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode status =
        pw::bluetooth::emboss::StatusCode::SUCCESS);

DynamicByteBuffer ConnectionRequestPacket(
    DeviceAddress address,
    hci_spec::LinkType link_type = hci_spec::LinkType::kACL);

DynamicByteBuffer CreateConnectionPacket(DeviceAddress address);

DynamicByteBuffer DisconnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode reason =
        pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
DynamicByteBuffer DisconnectPacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::StatusCode reason =
        pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
DynamicByteBuffer DisconnectStatusResponsePacket();

DynamicByteBuffer EmptyCommandPacket(hci_spec::OpCode opcode);

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

DynamicByteBuffer LEReadRemoteFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, hci_spec::LESupportedFeatures le_features);
DynamicByteBuffer LEReadRemoteFeaturesPacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer LECISRequestEventPacket(
    hci_spec::ConnectionHandle acl_connection_handle,
    hci_spec::ConnectionHandle cis_connection_handle,
    uint8_t cig_id,
    uint8_t cis_id);

DynamicByteBuffer LEAcceptCISRequestCommandPacket(
    hci_spec::ConnectionHandle cis_handle);

DynamicByteBuffer LERejectCISRequestCommandPacket(
    hci_spec::ConnectionHandle cis_handle,
    pw::bluetooth::emboss::StatusCode reason);

DynamicByteBuffer LERequestPeerScaCompletePacket(
    hci_spec::ConnectionHandle conn,
    pw::bluetooth::emboss::LESleepClockAccuracyRange sca);
DynamicByteBuffer LERequestPeerScaPacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer LEStartEncryptionPacket(hci_spec::ConnectionHandle,
                                          uint64_t random_number,
                                          uint16_t encrypted_diversifier,
                                          UInt128 ltk);

DynamicByteBuffer NumberOfCompletedPacketsPacket(
    hci_spec::ConnectionHandle conn, uint16_t num_packets);

// The ReadRemoteExtended*CompletePacket packets report a max page number of 3,
// even though there are only 2 pages, in order to test this behavior seen in
// real devices.
DynamicByteBuffer ReadRemoteExtended1CompletePacket(
    hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended1Packet(hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2CompletePacket(
    hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteExtended2Packet(hci_spec::ConnectionHandle conn);

DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(
    hci_spec::ConnectionHandle conn);
DynamicByteBuffer ReadRemoteVersionInfoPacket(hci_spec::ConnectionHandle conn);

DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(
    hci_spec::ConnectionHandle conn, bool extended_features);
DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(
    hci_spec::ConnectionHandle conn);

DynamicByteBuffer ReadScanEnable();
DynamicByteBuffer ReadScanEnableResponse(uint8_t scan_enable);

DynamicByteBuffer RejectConnectionRequestPacket(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode reason);

DynamicByteBuffer RejectSynchronousConnectionRequest(
    DeviceAddress address, pw::bluetooth::emboss::StatusCode status_code);

DynamicByteBuffer RemoteNameRequestCompletePacket(
    DeviceAddress address, const std::string& name = "Fuchsia💖");
DynamicByteBuffer RemoteNameRequestPacket(DeviceAddress address);

DynamicByteBuffer RoleChangePacket(
    DeviceAddress address,
    pw::bluetooth::emboss::ConnectionRole role,
    pw::bluetooth::emboss::StatusCode status =
        pw::bluetooth::emboss::StatusCode::SUCCESS);

DynamicByteBuffer ScoDataPacket(
    hci_spec::ConnectionHandle conn,
    hci_spec::SynchronousDataPacketStatusFlag flag,
    const BufferView& payload,
    std::optional<uint8_t> payload_length_override = std::nullopt);

DynamicByteBuffer SetConnectionEncryption(hci_spec::ConnectionHandle conn,
                                          bool enable);

DynamicByteBuffer StartA2dpOffloadRequest(
    const l2cap::A2dpOffloadManager::Configuration& config,
    hci_spec::ConnectionHandle connection_handle,
    l2cap::ChannelId l2cap_channel_id,
    uint16_t l2cap_mtu_size);

DynamicByteBuffer StopA2dpOffloadRequest();

DynamicByteBuffer SynchronousConnectionCompletePacket(
    hci_spec::ConnectionHandle conn,
    DeviceAddress address,
    hci_spec::LinkType link_type,
    pw::bluetooth::emboss::StatusCode status);

DynamicByteBuffer WriteAutomaticFlushTimeoutPacket(
    hci_spec::ConnectionHandle conn, uint16_t flush_timeout);

DynamicByteBuffer WriteInquiryScanActivity(uint16_t scan_interval,
                                           uint16_t scan_window);
DynamicByteBuffer WriteInquiryScanActivityResponse();

DynamicByteBuffer WritePageTimeoutPacket(uint16_t page_timeout);

DynamicByteBuffer WriteScanEnable(uint8_t scan_enable);
DynamicByteBuffer WriteScanEnableResponse();

}  // namespace bt::testing
