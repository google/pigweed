// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt {
namespace testing {

// This module contains functionality to create arbitrary HCI packets defining
// common behaviors with respect to expected devices and connections.
// This allows easily defining expected packets to be sent or received for
// given transactions such as connection establishment or discovery

common::DynamicByteBuffer CreateConnectionPacket(common::DeviceAddress address);
common::DynamicByteBuffer ConnectionCompletePacket(
    common::DeviceAddress address, hci::ConnectionHandle conn);
common::DynamicByteBuffer DisconnectPacket(hci::ConnectionHandle conn);
common::DynamicByteBuffer DisconnectionCompletePacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer RemoteNameRequestPacket(
    common::DeviceAddress address);
common::DynamicByteBuffer RemoteNameRequestCompletePacket(
    common::DeviceAddress address);
common::DynamicByteBuffer ReadRemoteVersionInfoPacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteVersionInfoCompletePacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteSupportedFeaturesPacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteSupportedFeaturesCompletePacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteExtended1Packet(hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteExtended1CompletePacket(
    hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteExtended2Packet(hci::ConnectionHandle conn);
common::DynamicByteBuffer ReadRemoteExtended2CompletePacket(
    hci::ConnectionHandle conn);

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_TEST_PACKETS_H_
