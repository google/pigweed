// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::hci {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
const hci_spec::LEConnectionParameters kTestParams(1, 1, 1);
const DeviceAddress kLEAddress1(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kLEAddress2(DeviceAddress::Type::kLEPublic, {2});
const DeviceAddress kACLAddress1(DeviceAddress::Type::kBREDR, {3});
const DeviceAddress kACLAddress2(DeviceAddress::Type::kBREDR, {4});

constexpr UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
constexpr uint64_t kRand = 1;
constexpr uint16_t kEDiv = 255;
constexpr hci_spec::LinkKeyType kLinkKeyType = hci_spec::LinkKeyType::kAuthenticatedCombination256;

const DataBufferInfo kBrEdrBufferInfo(1024, 5);
const DataBufferInfo kLeBufferInfo(1024, 1);

using bt::testing::CommandTransaction;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class ConnectionTest : public TestingBase {
 public:
  ConnectionTest() = default;
  ~ConnectionTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel(kBrEdrBufferInfo, kLeBufferInfo);
    StartTestDevice();
  }

  std::unique_ptr<LowEnergyConnection> NewLEConnection(
      hci_spec::ConnectionRole role = hci_spec::ConnectionRole::kCentral,
      hci_spec::ConnectionHandle handle = kTestHandle) {
    return std::make_unique<LowEnergyConnection>(handle, kLEAddress1, kLEAddress2, kTestParams,
                                                 role, transport()->WeakPtr());
  }

  std::unique_ptr<BrEdrConnection> NewACLConnection(
      hci_spec::ConnectionRole role = hci_spec::ConnectionRole::kCentral,
      hci_spec::ConnectionHandle handle = kTestHandle) {
    return std::make_unique<BrEdrConnection>(handle, kACLAddress1, kACLAddress2, role,
                                             transport()->WeakPtr());
  }
};

// Tests using this harness will be instantiated using ACL and LE link types.
// See INSTANTIATE_TEST_SUITE_P(ConnectionTest, LinkTypeConnectionTest, ...)
class LinkTypeConnectionTest : public ConnectionTest,
                               public ::testing::WithParamInterface<bt::LinkType> {
 protected:
  std::unique_ptr<AclConnection> NewConnection(
      hci_spec::ConnectionRole role = hci_spec::ConnectionRole::kCentral,
      hci_spec::ConnectionHandle handle = kTestHandle) {
    const bt::LinkType ll_type = GetParam();
    switch (ll_type) {
      case bt::LinkType::kACL:
        return NewACLConnection(role, handle);
      case bt::LinkType::kLE:
        return NewLEConnection(role, handle);
      default:
        break;
    }
    BT_PANIC("Invalid link type: %u", static_cast<unsigned>(ll_type));
    return nullptr;
  }

  // Assigns the appropriate test link key based on the type of link being tested.
  void SetTestLinkKey(Connection* connection) {
    const bt::LinkType ll_type = GetParam();
    if (ll_type == bt::LinkType::kLE) {
      static_cast<LowEnergyConnection*>(connection)->set_ltk(hci_spec::LinkKey(kLTK, kRand, kEDiv));
    } else {
      static_cast<BrEdrConnection*>(connection)
          ->set_link_key(hci_spec::LinkKey(kLTK, 0, 0), kLinkKeyType);
    }
  }
};

using HCI_ConnectionTest = ConnectionTest;

TEST_F(ConnectionTest, Getters) {
  std::unique_ptr<LowEnergyConnection> connection = NewLEConnection();

  EXPECT_EQ(kTestHandle, connection->handle());
  EXPECT_EQ(hci_spec::ConnectionRole::kCentral, connection->role());
  EXPECT_EQ(kTestParams, connection->low_energy_parameters());
  EXPECT_EQ(kLEAddress1, connection->local_address());
  EXPECT_EQ(kLEAddress2, connection->peer_address());

  EXPECT_EQ(std::nullopt, connection->ltk());
  connection->set_ltk(hci_spec::LinkKey());
  ASSERT_TRUE(connection->ltk().has_value());
  EXPECT_EQ(hci_spec::LinkKey(), connection->ltk().value());

  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_F(ConnectionTest, AclLinkKeyAndTypeAccessors) {
  std::unique_ptr<BrEdrConnection> connection = NewACLConnection();

  EXPECT_EQ(std::nullopt, connection->ltk());
  EXPECT_EQ(std::nullopt, connection->ltk_type());
  connection->set_link_key(hci_spec::LinkKey(), kLinkKeyType);
  ASSERT_TRUE(connection->ltk().has_value());
  EXPECT_EQ(hci_spec::LinkKey(), connection->ltk().value());
  ASSERT_TRUE(connection->ltk_type().has_value());
  EXPECT_EQ(kLinkKeyType, connection->ltk_type().value());

  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_P(LinkTypeConnectionTest, Disconnect) {
  // clang-format off

  // HCI_Disconnect (handle: 0x0001, reason: RemoteUserTerminatedConnection)
  StaticByteBuffer req_bytes(
      0x06, 0x04, 0x03, 0x01, 0x00, hci_spec::StatusCode::kRemoteUserTerminatedConnection);

  // Respond with Command Status and Disconnection Complete.
  StaticByteBuffer cmd_status_bytes(
      hci_spec::kCommandStatusEventCode, 0x04, hci_spec::StatusCode::kSuccess, 1, 0x06, 0x04);

  StaticByteBuffer disc_cmpl_bytes(
      hci_spec::kDisconnectionCompleteEventCode, 0x04,
      hci_spec::StatusCode::kSuccess, 0x01, 0x00, hci_spec::StatusCode::kConnectionTerminatedByLocalHost);

  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_bytes, &cmd_status_bytes, &disc_cmpl_bytes);

  bool callback_called = false;
  test_device()->SetTransactionCallback([&callback_called] { callback_called = true; },
                                        dispatcher());

  auto connection = NewConnection();

  size_t disconn_cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn, auto reason) {
    disconn_cb_count++;
    EXPECT_EQ(reason, hci_spec::StatusCode::kConnectionTerminatedByLocalHost);
  };
  connection->set_peer_disconnect_callback(disconn_complete_cb);

  connection->Disconnect(hci_spec::StatusCode::kRemoteUserTerminatedConnection);

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(1u, disconn_cb_count);
}

TEST_P(LinkTypeConnectionTest, LinkRegistrationAndLocalDisconnection) {
  const bt::LinkType ll_type = GetParam();
  const hci_spec::ConnectionHandle kHandle0 = 0x0001;
  const hci_spec::ConnectionHandle kHandle1 = 0x0002;

  const auto& kBufferInfo = ll_type == bt::LinkType::kACL ? kBrEdrBufferInfo : kLeBufferInfo;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewConnection(hci_spec::ConnectionRole::kCentral, kHandle0);
  auto conn1 = NewConnection(hci_spec::ConnectionRole::kCentral, kHandle1);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Fill controller buffer.
  for (size_t i = 0; i < kBufferInfo.max_num_packets(); i++) {
    // Connection handle should have been registered with ACL Data Channel.
    EXPECT_TRUE(acl_data_channel()->SendPacket(
        ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                           hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  }

  EXPECT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  EXPECT_EQ(handle0_packet_count, kBufferInfo.max_num_packets());
  EXPECT_EQ(handle1_packet_count, 0u);

  const auto disconnect_status_rsp = bt::testing::DisconnectStatusResponsePacket();
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kHandle0),
                        &disconnect_status_rsp);

  conn0->Disconnect(hci_spec::StatusCode::kRemoteUserTerminatedConnection);
  RunLoopUntilIdle();

  // controller packet counts for |kHandle0| should not have been cleared after disconnect.
  EXPECT_EQ(handle1_packet_count, 0u);

  // Disconnection Complete handler should clear controller packet counts, so packet for |kHandle1|
  // should be sent.
  DynamicByteBuffer disconnection_complete(bt::testing::DisconnectionCompletePacket(kHandle0));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(handle1_packet_count, 1u);

  // Connection handle should have been unregistered with ACL Data Channel.
  EXPECT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kHandle1));
}

// In remote disconnection, Connection::Disconnect is not called. Instead,
// Connection::OnDisconnectionComplete is invoked and handles all cleanup.
TEST_P(LinkTypeConnectionTest, LinkRegistrationAndRemoteDisconnection) {
  const bt::LinkType ll_type = GetParam();
  const hci_spec::ConnectionHandle kHandle0 = 0x0001;
  const hci_spec::ConnectionHandle kHandle1 = 0x0002;

  const auto& kBufferInfo = ll_type == bt::LinkType::kACL ? kBrEdrBufferInfo : kLeBufferInfo;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewConnection(hci_spec::ConnectionRole::kCentral, kHandle0);

  auto conn1 = NewConnection(hci_spec::ConnectionRole::kCentral, kHandle1);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Fill controller buffer.
  for (size_t i = 0; i < kBufferInfo.max_num_packets(); i++) {
    // Connection handle should have been registered with ACL Data Channel.
    EXPECT_TRUE(acl_data_channel()->SendPacket(
        ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                           hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  }

  EXPECT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  EXPECT_EQ(handle0_packet_count, kBufferInfo.max_num_packets());
  EXPECT_EQ(handle1_packet_count, 0u);

  size_t disconn_cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn, auto /*reason*/) {
    ASSERT_TRUE(cb_conn);
    EXPECT_EQ(kHandle0, cb_conn->handle());
    disconn_cb_count++;
  };
  conn0->set_peer_disconnect_callback(disconn_complete_cb);

  // Disconnection Complete handler should clear controller packet counts, so packet for |kHandle1|
  // should be sent.
  DynamicByteBuffer disconnection_complete(bt::testing::DisconnectionCompletePacket(kHandle0));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, disconn_cb_count);

  // Connection handle should have been unregistered with ACL Data Channel.
  EXPECT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  // Since controller packet count was cleared, packet for |kHandle1| should
  // have been sent.
  EXPECT_EQ(handle1_packet_count, 1u);

  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kHandle1));
}

TEST_F(ConnectionTest, StartEncryptionFailsAsLowEnergyPeripheral) {
  auto conn = NewLEConnection(hci_spec::ConnectionRole::kPeripheral);
  conn->set_ltk(hci_spec::LinkKey());
  EXPECT_FALSE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_F(ConnectionTest, StartEncryptionSucceedsAsLowEnergyCentral) {
  auto conn = NewLEConnection(hci_spec::ConnectionRole::kCentral);
  auto ltk = hci_spec::LinkKey();
  conn->set_ltk(ltk);
  EXPECT_TRUE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::LEStartEncryptionPacket(
                                           kTestHandle, ltk.rand(), ltk.ediv(), ltk.value()));
}

TEST_F(ConnectionTest, StartEncryptionSucceedsWithBrEdrLinkKeyType) {
  auto conn = NewACLConnection();
  conn->set_link_key(hci_spec::LinkKey(), kLinkKeyType);
  EXPECT_TRUE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(),
                        bt::testing::SetConnectionEncryption(kTestHandle, /*enable=*/true));
}

TEST_P(LinkTypeConnectionTest, DisconnectError) {
  // clang-format off

  // HCI_Disconnect (handle: 0x0001, reason: RemoteUserTerminatedConnection)
  StaticByteBuffer req_bytes(
      0x06, 0x04, 0x03, 0x01, 0x00, hci_spec::StatusCode::kRemoteUserTerminatedConnection);

  // Respond with Command Status and Disconnection Complete.
  StaticByteBuffer cmd_status_bytes(
      hci_spec::kCommandStatusEventCode, 0x04, hci_spec::StatusCode::kSuccess, 1, 0x06, 0x04);

  StaticByteBuffer disc_cmpl_bytes(
      hci_spec::kDisconnectionCompleteEventCode, 0x04,
      hci_spec::StatusCode::kCommandDisallowed, 0x01, 0x00, hci_spec::StatusCode::kConnectionTerminatedByLocalHost);

  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_bytes, &cmd_status_bytes, &disc_cmpl_bytes);

  // The callback should get called regardless of the procedure status.
  bool callback_called = false;
  test_device()->SetTransactionCallback([&callback_called] { callback_called = true; },
                                        dispatcher());

  auto connection = NewConnection();

  connection->Disconnect(hci_spec::StatusCode::kRemoteUserTerminatedConnection);

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_P(LinkTypeConnectionTest, StartEncryptionNoLinkKey) {
  auto conn = NewConnection();
  EXPECT_FALSE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

// HCI Command Status event is received with an error status.
TEST_F(ConnectionTest, LEStartEncryptionFailsAtStatus) {
  // clang-format off
  StaticByteBuffer kExpectedCommand(
    0x19, 0x20,  // HCI_LE_Start_Encryption
    28,          // parameter total size
    0x01, 0x00,  // connection handle: 1
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // rand: 1
    0xFF, 0x00,  // ediv: 255
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16  // LTK
  );
  StaticByteBuffer kErrorStatus(
    0x0F,       // HCI Command Status event code
    4,          // parameter total size
    0x0C,       // "Command Disallowed" error
    1,          // num_hci_command_packets
    0x19, 0x20  // opcode: HCI_LE_Start_Encryption
  );
  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kErrorStatus);

  bool callback = false;
  auto conn = NewLEConnection();
  conn->set_ltk(hci_spec::LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](Result<bool> result) {
    ASSERT_TRUE(result.is_error());
    EXPECT_TRUE(result.error_value().is(hci_spec::StatusCode::kCommandDisallowed));
    callback = true;
  });

  EXPECT_TRUE(conn->StartEncryption());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_F(ConnectionTest, LEStartEncryptionSendsSetLeConnectionEncryptionCommand) {
  StaticByteBuffer kExpectedCommand(0x19, 0x20,  // HCI_LE_Start_Encryption
                                    28,          // parameter total size
                                    0x01, 0x00,  // connection handle: 1
                                    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // rand: 1
                                    0xFF, 0x00,                                      // ediv: 255
                                    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16  // LTK
  );
  StaticByteBuffer kStatus(0x0F,       // HCI Command Status event code
                           4,          // parameter total size
                           0x00,       // success status
                           1,          // num_hci_command_packets
                           0x19, 0x20  // opcode: HCI_LE_Start_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kStatus);

  bool callback = false;
  auto conn = NewLEConnection();
  conn->set_ltk(hci_spec::LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](auto) { callback = true; });

  EXPECT_TRUE(conn->StartEncryption());

  // Callback shouldn't be called until the controller sends an encryption
  // changed event.
  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

// HCI Command Status event is received with an error status.
TEST_F(ConnectionTest, AclStartEncryptionFailsAtStatus) {
  StaticByteBuffer kExpectedCommand(0x13, 0x04,  // HCI_Set_Connection_Encryption
                                    3,           // parameter total size
                                    0x01, 0x00,  // connection handle
                                    0x01         // encryption enable
  );
  StaticByteBuffer kErrorStatus(0x0F,       // HCI Command Status event code
                                4,          // parameter total size
                                0x0C,       // "Command Disallowed" error
                                1,          // num_hci_command_packets
                                0x13, 0x04  // opcode: HCI_Set_Connection_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kErrorStatus);

  bool callback = false;
  auto conn = NewACLConnection();
  conn->set_link_key(hci_spec::LinkKey(kLTK, 0, 0), kLinkKeyType);
  conn->set_encryption_change_callback([&](Result<bool> result) {
    ASSERT_TRUE(result.is_error());
    EXPECT_TRUE(result.error_value().is(hci_spec::StatusCode::kCommandDisallowed));
    callback = true;
  });

  EXPECT_TRUE(conn->StartEncryption());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_F(ConnectionTest, AclStartEncryptionSendsSetConnectionEncryptionCommand) {
  StaticByteBuffer kExpectedCommand(0x13, 0x04,  // HCI_Set_Connection_Encryption
                                    3,           // parameter total size
                                    0x01, 0x00,  // connection handle
                                    0x01         // encryption enable
  );
  StaticByteBuffer kStatus(0x0F,       // HCI Command Status event code
                           4,          // parameter total size
                           0x00,       // success status
                           1,          // num_hci_command_packets
                           0x13, 0x04  // opcode: HCI_Set_Connection_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kStatus);

  bool callback = false;
  auto conn = NewACLConnection();
  conn->set_link_key(hci_spec::LinkKey(kLTK, 0, 0), kLinkKeyType);
  conn->set_encryption_change_callback([&](auto) { callback = true; });

  EXPECT_TRUE(conn->StartEncryption());

  // Callback shouldn't be called until the controller sends an encryption changed event.
  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_P(LinkTypeConnectionTest, EncryptionChangeIgnoredEvents) {
  // clang-format off
  StaticByteBuffer kEncChangeMalformed(
    0x08,       // HCI Encryption Change event code
    3,          // parameter total size
    0x00,       // status
    0x01, 0x00  // connection handle: 1
    // Last byte missing
  );
  StaticByteBuffer kEncChangeWrongHandle(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x00,        // status
    0x02, 0x00,  // connection handle: 2
    0x01         // encryption enabled
  );
  // clang-format on

  bool callback = false;
  auto conn = NewConnection();
  SetTestLinkKey(conn.get());
  conn->set_encryption_change_callback([&](auto) { callback = true; });

  test_device()->SendCommandChannelPacket(kEncChangeMalformed);
  test_device()->SendCommandChannelPacket(kEncChangeWrongHandle);

  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

const StaticByteBuffer kEncryptionChangeEventEnabled(0x08,  // HCI Encryption Change event code
                                                     4,     // parameter total size
                                                     0x00,  // status
                                                     0x01, 0x00,  // connection handle: 1
                                                     0x01         // encryption enabled
);

const auto kReadEncryptionKeySizeCommand =
    StaticByteBuffer(0x08, 0x14,  // opcode: HCI_ReadEncryptionKeySize
                     0x02,        // parameter size
                     0x01, 0x00   // connection handle: 0x0001
    );

const StaticByteBuffer kDisconnectCommand(0x06, 0x04,  // opcode: HCI_Disconnect
                                          0x03,        // parameter total size
                                          0x01, 0x00,  // handle: 1
                                          0x05         // reason: authentication failure
);

TEST_P(LinkTypeConnectionTest, EncryptionChangeEvents) {
  // clang-format off
  StaticByteBuffer kEncryptionChangeEventDisabled(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x00,        // status
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );
  StaticByteBuffer kEncryptionChangeEventFailed(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x06,        // status: Pin or Key missing
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );

  StaticByteBuffer kKeySizeComplete(
    0x0E,        // event code: Command Complete
    0x07,        // parameters total size
    0xFF,        // num command packets allowed (255)
    0x08, 0x14,  // original opcode

    // return parameters
    0x00,        // status (success)
    0x01, 0x00,  // connection handle: 0x0001
    0x10         // encryption key size: 16
  );
  // clang-format on

  int callback_count = 0;
  auto conn = NewConnection();

  Result<bool> result = fit::error(Error(HostError::kFailed));
  conn->set_encryption_change_callback([&](Result<bool> cb_result) {
    callback_count++;
    result = cb_result;
  });

  if (GetParam() == bt::LinkType::kACL) {
    // The host tries to validate the size of key used to encrypt ACL links.
    EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  }

  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(fit::ok(), result);
  EXPECT_TRUE(result.value_or(false));

  test_device()->SendCommandChannelPacket(kEncryptionChangeEventDisabled);
  RunLoopUntilIdle();

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(fit::ok(), result);
  EXPECT_FALSE(result.value_or(true));

  // The host should disconnect the link if encryption fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(3, callback_count);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kPinOrKeyMissing).error_value(), result);
}

TEST_F(ConnectionTest, EncryptionFailureNotifiesPeerDisconnectCallback) {
  bool peer_disconnect_callback_received = false;
  auto conn = NewLEConnection();
  conn->set_peer_disconnect_callback([&](auto* self, auto /*reason*/) {
    EXPECT_EQ(conn.get(), self);
    peer_disconnect_callback_received = true;
  });

  // Send the encryption change failure. The host should disconnect the link as a result.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(bt::testing::EncryptionChangeEventPacket(
      hci_spec::StatusCode::kConnectionTerminatedMICFailure, kTestHandle,
      hci_spec::EncryptionStatus::kOff));
  RunLoopUntilIdle();
  EXPECT_FALSE(peer_disconnect_callback_received);

  // Send the disconnection complete resulting from the encryption failure (this usually does not
  // correspond to the Disconnect command sent by hci::Connection, which will cause a later
  // subsequent event).
  test_device()->SendCommandChannelPacket(bt::testing::DisconnectionCompletePacket(
      kTestHandle, hci_spec::StatusCode::kConnectionTerminatedMICFailure));
  RunLoopUntilIdle();
  EXPECT_TRUE(peer_disconnect_callback_received);
}

TEST_F(ConnectionTest, AclEncryptionEnableCanNotReadKeySizeClosesLink) {
  StaticByteBuffer kKeySizeComplete(0x0E,        // event code: Command Complete
                                    0x07,        // parameters total size
                                    0xFF,        // num command packets allowed (255)
                                    0x08, 0x14,  // original opcode

                                    // return parameters
                                    0x2F,        // status (insufficient security)
                                    0x01, 0x00,  // connection handle: 0x0001
                                    0x10         // encryption key size: 16
  );

  int callback_count = 0;
  auto conn = NewACLConnection();
  conn->set_encryption_change_callback([&callback_count](Result<bool> result) {
    callback_count++;
    EXPECT_TRUE(result.is_error());
  });

  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
}

TEST_F(ConnectionTest, AclEncryptionEnableKeySizeOneByteClosesLink) {
  StaticByteBuffer kKeySizeComplete(0x0E,        // event code: Command Complete
                                    0x07,        // parameters total size
                                    0xFF,        // num command packets allowed (255)
                                    0x08, 0x14,  // original opcode

                                    // return parameters
                                    0x00,        // status (success)
                                    0x01, 0x00,  // connection handle: 0x0001
                                    0x01         // encryption key size: 1
  );

  int callback_count = 0;
  auto conn = NewACLConnection();
  conn->set_encryption_change_callback([&callback_count](Result<bool> result) {
    callback_count++;
    EXPECT_TRUE(result.is_error());
  });

  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
}

TEST_P(LinkTypeConnectionTest, EncryptionKeyRefreshEvents) {
  // clang-format off
  StaticByteBuffer kEncryptionKeyRefresh(
    0x30,       // HCI Encryption Key Refresh Complete event
    3,          // parameter total size
    0x00,       // status
    0x01, 0x00  // connection handle: 1
  );
  StaticByteBuffer kEncryptionKeyRefreshFailed(
    0x30,       // HCI Encryption Key Refresh Complete event
    3,          // parameter total size
    0x06,       // status: Pin or Key missing
    0x01, 0x00  // connection handle: 1
  );
  // clang-format on

  int callback_count = 0;
  auto conn = NewConnection();

  Result<bool> result = fit::error(Error(HostError::kFailed));
  conn->set_encryption_change_callback([&](Result<bool> cb_result) {
    callback_count++;
    result = cb_result;
  });

  test_device()->SendCommandChannelPacket(kEncryptionKeyRefresh);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
  ASSERT_EQ(fit::ok(), result);
  EXPECT_TRUE(result.value());

  // The host should disconnect the link if encryption fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionKeyRefreshFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(ToResult(hci_spec::StatusCode::kPinOrKeyMissing).error_value(), result);
}

TEST_F(ConnectionTest, LELongTermKeyRequestIgnoredEvent) {
  // clang-format off
  StaticByteBuffer kMalformed(
    0x3E,        // LE Meta Event code
    12,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 1

    // rand:
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: (missing 1 byte)
    0x00
  );
  StaticByteBuffer kWrongHandle(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x02, 0x00,  // connection handle: 2 (wrong)

    // rand: 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: 0
    0x00, 0x00
  );
  // clang-format on

  auto conn = NewLEConnection();
  conn->set_ltk(hci_spec::LinkKey(kLTK, 0, 0));

  test_device()->SendCommandChannelPacket(kMalformed);
  test_device()->SendCommandChannelPacket(kWrongHandle);

  RunLoopUntilIdle();

  // Test will fail if the connection sends a response without ignoring these
  // events.
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kTestHandle));
}

TEST_F(ConnectionTest, LELongTermKeyRequestNoKey) {
  // clang-format off
  StaticByteBuffer kEvent(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: 0
    0x00, 0x00
  );
  StaticByteBuffer kResponse(
    0x1B, 0x20,  // opcode: HCI_LE_Long_Term_Key_Request_Negative_Reply
    2,           // parameter total size
    0x01, 0x00   // connection handle: 1
  );
  // clang-format on

  // The request should be rejected since there is no LTK.
  EXPECT_CMD_PACKET_OUT(test_device(), kResponse);
  auto conn = NewLEConnection();

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

// There is a link key but EDiv and Rand values don't match.
TEST_F(ConnectionTest, LELongTermKeyRequestNoMatchinKey) {
  // clang-format off
  StaticByteBuffer kEvent(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: 0
    0x00, 0x00
  );
  StaticByteBuffer kResponse(
    0x1B, 0x20,  // opcode: HCI_LE_Long_Term_Key_Request_Negative_Reply
    2,           // parameter total size
    0x01, 0x00   // connection handle: 1
  );
  // clang-format on

  // The request should be rejected since there is no LTK.
  EXPECT_CMD_PACKET_OUT(test_device(), kResponse);
  auto conn = NewLEConnection();
  conn->set_ltk(hci_spec::LinkKey(kLTK, 1, 1));

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

TEST_F(ConnectionTest, LELongTermKeyRequestReply) {
  // clang-format off
  StaticByteBuffer kEvent(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0x8899AABBCCDDEEFF
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    // ediv: 0xBEEF
    0xEF, 0xBE
  );
  StaticByteBuffer kResponse(
    0x1A, 0x20,  // opcode: HCI_LE_Long_Term_Key_Request_Reply
    18,          // parameter total size
    0x01, 0x00,  // connection handle: 1

    // LTK:
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  // The request should be rejected since there is no LTK.
  EXPECT_CMD_PACKET_OUT(test_device(), kResponse);
  auto conn = NewLEConnection();
  conn->set_ltk(hci_spec::LinkKey(kLTK, 0x8899AABBCCDDEEFF, 0xBEEF));

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

TEST_F(ConnectionTest,
       QueuedPacketsGetDroppedOnDisconnectionCompleteAndStalePacketsAreNotSentOnHandleReuse) {
  const hci_spec::ConnectionHandle kHandle = 0x0001;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewACLConnection(hci_spec::ConnectionRole::kCentral, kHandle);

  bt::testing::MockController::DataCallback data_cb = [](const ByteBuffer& packet) {};
  size_t packet_count = 0;
  auto data_cb_wrapper = [&data_cb, &packet_count](const ByteBuffer& packet) {
    packet_count++;
    ASSERT_EQ(packet.size(), sizeof(hci_spec::ACLDataHeader) + 1);
    data_cb(packet);
  };
  test_device()->SetDataCallback(data_cb_wrapper, dispatcher());

  const uint8_t payload0 = 0x01;
  data_cb = [payload0](const ByteBuffer& packet) {
    EXPECT_EQ(packet[sizeof(hci_spec::ACLDataHeader)], payload0);
  };

  // Fill controller buffer, + 1 packet in queue.
  for (size_t i = 0; i < kBrEdrBufferInfo.max_num_packets() + 1; i++) {
    auto packet = ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = payload0;
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }

  // Run until the data is flushed out to the MockController.
  RunLoopUntilIdle();

  // Only packets that fit in buffer should have been received.
  EXPECT_EQ(packet_count, kBrEdrBufferInfo.max_num_packets());

  // All future packets received should be for the next connection.
  const uint8_t payload1 = 0x02;
  data_cb = [payload1](const ByteBuffer& packet) {
    EXPECT_EQ(packet[sizeof(hci_spec::ACLDataHeader)], payload1);
  };

  const auto disconnect_status_rsp = bt::testing::DisconnectStatusResponsePacket();
  DynamicByteBuffer disconnection_complete(bt::testing::DisconnectionCompletePacket(kHandle));
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kHandle),
                        &disconnect_status_rsp, &disconnection_complete);

  // Disconnect |conn0| by destroying it. The received disconnection complete event will cause the
  // handler to unregister the link and clear pending packets.
  conn0.reset();
  RunLoopUntilIdle();

  // Register connection with same handle.
  auto conn1 = NewACLConnection(hci_spec::ConnectionRole::kCentral, kHandle);

  // Fill controller buffer, + 1 packet in queue.
  for (size_t i = 0; i < kBrEdrBufferInfo.max_num_packets(); i++) {
    auto packet = ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = payload1;
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }

  RunLoopUntilIdle();

  EXPECT_EQ(packet_count, 2 * kBrEdrBufferInfo.max_num_packets());

  conn1.reset();
  EXPECT_CMD_PACKET_OUT(test_device(), bt::testing::DisconnectPacket(kHandle),
                        &disconnect_status_rsp, &disconnection_complete);
  RunLoopUntilIdle();
}

TEST_F(ConnectionTest, PeerDisconnectCallback) {
  const hci_spec::ConnectionHandle kHandle = 0x0001;

  auto conn = NewACLConnection(hci_spec::ConnectionRole::kCentral, kHandle);

  size_t cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn, auto /*reason*/) {
    ASSERT_TRUE(cb_conn);
    cb_count++;

    // Should be safe to destroy connection from this callback, as a connection manager does.
    conn.reset();
  };
  conn->set_peer_disconnect_callback(disconn_complete_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(0u, cb_count);

  DynamicByteBuffer disconnection_complete(bt::testing::DisconnectionCompletePacket(kHandle));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, cb_count);
}

// Test connection handling cases for all types of links.
INSTANTIATE_TEST_SUITE_P(ConnectionTest, LinkTypeConnectionTest,
                         ::testing::Values(bt::LinkType::kACL, bt::LinkType::kLE));

}  // namespace
}  // namespace bt::hci
