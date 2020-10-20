// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt {
namespace hci {
namespace {

constexpr ConnectionHandle kTestHandle = 0x0001;
const LEConnectionParameters kTestParams(1, 1, 1);
const DeviceAddress kLEAddress1(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kLEAddress2(DeviceAddress::Type::kLEPublic, {2});
const DeviceAddress kACLAddress1(DeviceAddress::Type::kBREDR, {3});
const DeviceAddress kACLAddress2(DeviceAddress::Type::kBREDR, {4});

constexpr UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
constexpr uint64_t kRand = 1;
constexpr uint16_t kEDiv = 255;
constexpr LinkKeyType kLinkKeyType = LinkKeyType::kAuthenticatedCombination256;

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

  ConnectionPtr NewLEConnection(Connection::Role role = Connection::Role::kMaster,
                                ConnectionHandle handle = kTestHandle) {
    return Connection::CreateLE(handle, role, kLEAddress1, kLEAddress2, kTestParams,
                                transport()->WeakPtr());
  }

  ConnectionPtr NewACLConnection(Connection::Role role = Connection::Role::kMaster,
                                 ConnectionHandle handle = kTestHandle) {
    return Connection::CreateACL(handle, role, kACLAddress1, kACLAddress2, transport()->WeakPtr());
  }
};

// Tests using this harness will be instantiated using ACL and LE link types.
// See INSTANTIATE_TEST_SUITE_P(HCI_ConnectionTest, LinkTypeConnectionTest, ...)
class LinkTypeConnectionTest : public ConnectionTest,
                               public ::testing::WithParamInterface<Connection::LinkType> {
 protected:
  ConnectionPtr NewConnection(Connection::Role role = Connection::Role::kMaster,
                              ConnectionHandle handle = kTestHandle) {
    const Connection::LinkType ll_type = GetParam();
    switch (ll_type) {
      case Connection::LinkType::kACL:
        return NewACLConnection(role, handle);
      case Connection::LinkType::kLE:
        return NewLEConnection(role, handle);
      default:
        break;
    }
    ZX_PANIC("Invalid link type: %u", static_cast<unsigned>(ll_type));
    return nullptr;
  }

  // Assigns the appropriate test link key based on the type of link being tested.
  void SetTestLinkKey(Connection* connection) {
    const Connection::LinkType ll_type = GetParam();
    if (ll_type == Connection::LinkType::kLE) {
      connection->set_le_ltk(LinkKey(kLTK, kRand, kEDiv));
    } else {
      connection->set_bredr_link_key(LinkKey(kLTK, 0, 0), kLinkKeyType);
    }
  }
};

using HCI_ConnectionTest = ConnectionTest;

TEST_F(HCI_ConnectionTest, Getters) {
  auto connection = NewLEConnection();

  EXPECT_EQ(Connection::LinkType::kLE, connection->ll_type());
  EXPECT_EQ(kTestHandle, connection->handle());
  EXPECT_EQ(Connection::Role::kMaster, connection->role());
  EXPECT_EQ(kTestParams, connection->low_energy_parameters());
  EXPECT_EQ(kLEAddress1, connection->local_address());
  EXPECT_EQ(kLEAddress2, connection->peer_address());

  EXPECT_EQ(std::nullopt, connection->ltk());
  connection->set_le_ltk(LinkKey());
  ASSERT_TRUE(connection->ltk().has_value());
  EXPECT_EQ(LinkKey(), connection->ltk().value());

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_F(HCI_ConnectionTest, AclLinkKeyAndTypeAccessors) {
  auto connection = NewACLConnection();

  EXPECT_EQ(Connection::LinkType::kACL, connection->ll_type());
  EXPECT_EQ(std::nullopt, connection->ltk());
  EXPECT_EQ(std::nullopt, connection->ltk_type());
  connection->set_bredr_link_key(LinkKey(), kLinkKeyType);
  ASSERT_TRUE(connection->ltk().has_value());
  EXPECT_EQ(LinkKey(), connection->ltk().value());
  ASSERT_TRUE(connection->ltk_type().has_value());
  EXPECT_EQ(kLinkKeyType, connection->ltk_type().value());

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_P(LinkTypeConnectionTest, Disconnect) {
  // clang-format off

  // HCI_Disconnect (handle: 0x0001, reason: RemoteUserTerminatedConnection)
  auto req_bytes = CreateStaticByteBuffer(
      0x06, 0x04, 0x03, 0x01, 0x00, StatusCode::kRemoteUserTerminatedConnection);

  // Respond with Command Status and Disconnection Complete.
  auto cmd_status_bytes = CreateStaticByteBuffer(
      kCommandStatusEventCode, 0x04, StatusCode::kSuccess, 1, 0x06, 0x04);

  auto disc_cmpl_bytes = CreateStaticByteBuffer(
      kDisconnectionCompleteEventCode, 0x04,
      StatusCode::kSuccess, 0x01, 0x00, StatusCode::kConnectionTerminatedByLocalHost);

  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_bytes, &cmd_status_bytes, &disc_cmpl_bytes);

  bool callback_called = false;
  test_device()->SetTransactionCallback([&callback_called] { callback_called = true; },
                                        dispatcher());

  auto connection = NewConnection();

  size_t disconn_cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn) { disconn_cb_count++; };
  connection->set_peer_disconnect_callback(disconn_complete_cb);

  connection->Disconnect(StatusCode::kRemoteUserTerminatedConnection);

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(1u, disconn_cb_count);
}

TEST_P(LinkTypeConnectionTest, LinkRegistrationAndLocalDisconnection) {
  const Connection::LinkType ll_type = GetParam();
  const ConnectionHandle kHandle0 = 0x0001;
  const ConnectionHandle kHandle1 = 0x0002;

  const auto& kBufferInfo =
      ll_type == Connection::LinkType::kACL ? kBrEdrBufferInfo : kLeBufferInfo;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewConnection(Connection::Role::kMaster, kHandle0);
  auto conn1 = NewConnection(Connection::Role::kMaster, kHandle1);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
        ACLDataPacket::New(kHandle0, ACLPacketBoundaryFlag::kFirstNonFlushable,
                           ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId));
  }

  EXPECT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  EXPECT_EQ(handle0_packet_count, kBufferInfo.max_num_packets());
  EXPECT_EQ(handle1_packet_count, 0u);

  const auto disconnect_status_rsp = testing::DisconnectStatusResponsePacket();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kHandle0), &disconnect_status_rsp);

  conn0->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
  RunLoopUntilIdle();

  // controller packet counts for |kHandle0| should not have been cleared after disconnect.
  EXPECT_EQ(handle1_packet_count, 0u);

  // Disconnection Complete handler should clear controller packet counts, so packet for |kHandle1|
  // should be sent.
  DynamicByteBuffer disconnection_complete(testing::DisconnectionCompletePacket(kHandle0));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(handle1_packet_count, 1u);

  // Connection handle should have been unregistered with ACL Data Channel.
  EXPECT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle0, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kHandle1));
}

// In remote disconnection, Connection::Disconnect is not called. Instead,
// Connection::OnDisconnectionComplete is invoked and handles all cleanup.
TEST_P(LinkTypeConnectionTest, LinkRegistrationAndRemoteDisconnection) {
  const Connection::LinkType ll_type = GetParam();
  const ConnectionHandle kHandle0 = 0x0001;
  const ConnectionHandle kHandle1 = 0x0002;

  const auto& kBufferInfo =
      ll_type == Connection::LinkType::kACL ? kBrEdrBufferInfo : kLeBufferInfo;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewConnection(Connection::Role::kMaster, kHandle0);

  auto conn1 = NewConnection(Connection::Role::kMaster, kHandle1);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
        ACLDataPacket::New(kHandle0, ACLPacketBoundaryFlag::kFirstNonFlushable,
                           ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId));
  }

  EXPECT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  EXPECT_EQ(handle0_packet_count, kBufferInfo.max_num_packets());
  EXPECT_EQ(handle1_packet_count, 0u);

  size_t disconn_cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn) {
    ASSERT_TRUE(cb_conn);
    EXPECT_EQ(kHandle0, cb_conn->handle());
    disconn_cb_count++;
  };
  conn0->set_peer_disconnect_callback(disconn_complete_cb);

  // Disconnection Complete handler should clear controller packet counts, so packet for |kHandle1|
  // should be sent.
  DynamicByteBuffer disconnection_complete(testing::DisconnectionCompletePacket(kHandle0));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, disconn_cb_count);

  // Connection handle should have been unregistered with ACL Data Channel.
  EXPECT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle0, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  // Since controller packet count was cleared, packet for |kHandle1| should
  // have been sent.
  EXPECT_EQ(handle1_packet_count, 1u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kHandle1));
}

TEST_F(HCI_ConnectionTest, StartEncryptionFailsAsLowEnergySlave) {
  auto conn = NewLEConnection(Connection::Role::kSlave);
  conn->set_le_ltk(LinkKey());
  EXPECT_FALSE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_F(HCI_ConnectionTest, StartEncryptionSucceedsAsLowEnergyMaster) {
  auto conn = NewLEConnection(Connection::Role::kMaster);
  auto ltk = LinkKey();
  conn->set_le_ltk(ltk);
  EXPECT_TRUE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEStartEncryptionPacket(kTestHandle, ltk.rand(),
                                                                        ltk.ediv(), ltk.value()));
}

TEST_F(HCI_ConnectionTest, StartEncryptionSucceedsWithBrEdrLinkKeyType) {
  auto conn = NewACLConnection();
  conn->set_bredr_link_key(LinkKey(), kLinkKeyType);
  EXPECT_TRUE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::SetConnectionEncryption(kTestHandle, /*enable=*/true));
}

TEST_P(LinkTypeConnectionTest, DisconnectError) {
  // clang-format off

  // HCI_Disconnect (handle: 0x0001, reason: RemoteUserTerminatedConnection)
  auto req_bytes = CreateStaticByteBuffer(
      0x06, 0x04, 0x03, 0x01, 0x00, StatusCode::kRemoteUserTerminatedConnection);

  // Respond with Command Status and Disconnection Complete.
  auto cmd_status_bytes = CreateStaticByteBuffer(
      kCommandStatusEventCode, 0x04, StatusCode::kSuccess, 1, 0x06, 0x04);

  auto disc_cmpl_bytes = CreateStaticByteBuffer(
      kDisconnectionCompleteEventCode, 0x04,
      StatusCode::kCommandDisallowed, 0x01, 0x00, StatusCode::kConnectionTerminatedByLocalHost);

  // clang-format on

  EXPECT_CMD_PACKET_OUT(test_device(), req_bytes, &cmd_status_bytes, &disc_cmpl_bytes);

  // The callback should get called regardless of the procedure status.
  bool callback_called = false;
  test_device()->SetTransactionCallback([&callback_called] { callback_called = true; },
                                        dispatcher());

  auto connection = NewConnection();

  connection->Disconnect(StatusCode::kRemoteUserTerminatedConnection);

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_P(LinkTypeConnectionTest, StartEncryptionNoLinkKey) {
  auto conn = NewConnection();
  EXPECT_FALSE(conn->StartEncryption());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

// HCI Command Status event is received with an error status.
TEST_F(HCI_ConnectionTest, LEStartEncryptionFailsAtStatus) {
  // clang-format off
  auto kExpectedCommand = CreateStaticByteBuffer(
    0x19, 0x20,  // HCI_LE_Start_Encryption
    28,          // parameter total size
    0x01, 0x00,  // connection handle: 1
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // rand: 1
    0xFF, 0x00,  // ediv: 255
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16  // LTK
  );
  auto kErrorStatus = CreateStaticByteBuffer(
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
  conn->set_le_ltk(LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](Status status, bool enabled) {
    EXPECT_FALSE(status);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(StatusCode::kCommandDisallowed, status.protocol_error());
    callback = true;
  });

  EXPECT_TRUE(conn->StartEncryption());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_F(HCI_ConnectionTest, LEStartEncryptionSendsSetLeConnectionEncryptionCommand) {
  auto kExpectedCommand =
      CreateStaticByteBuffer(0x19, 0x20,  // HCI_LE_Start_Encryption
                             28,          // parameter total size
                             0x01, 0x00,  // connection handle: 1
                             0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,        // rand: 1
                             0xFF, 0x00,                                            // ediv: 255
                             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16  // LTK
      );
  auto kStatus = CreateStaticByteBuffer(0x0F,       // HCI Command Status event code
                                        4,          // parameter total size
                                        0x00,       // success status
                                        1,          // num_hci_command_packets
                                        0x19, 0x20  // opcode: HCI_LE_Start_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kStatus);

  bool callback = false;
  auto conn = NewLEConnection();
  conn->set_le_ltk(LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](Status status, bool enabled) { callback = true; });

  EXPECT_TRUE(conn->StartEncryption());

  // Callback shouldn't be called until the controller sends an encryption
  // changed event.
  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

// HCI Command Status event is received with an error status.
TEST_F(HCI_ConnectionTest, AclStartEncryptionFailsAtStatus) {
  auto kExpectedCommand = CreateStaticByteBuffer(0x13, 0x04,  // HCI_Set_Connection_Encryption
                                                 3,           // parameter total size
                                                 0x01, 0x00,  // connection handle
                                                 0x01         // encryption enable
  );
  auto kErrorStatus = CreateStaticByteBuffer(0x0F,       // HCI Command Status event code
                                             4,          // parameter total size
                                             0x0C,       // "Command Disallowed" error
                                             1,          // num_hci_command_packets
                                             0x13, 0x04  // opcode: HCI_Set_Connection_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kErrorStatus);

  bool callback = false;
  auto conn = NewACLConnection();
  conn->set_bredr_link_key(LinkKey(kLTK, 0, 0), kLinkKeyType);
  conn->set_encryption_change_callback([&](Status status, bool enabled) {
    EXPECT_FALSE(status);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(StatusCode::kCommandDisallowed, status.protocol_error());
    callback = true;
  });

  EXPECT_TRUE(conn->StartEncryption());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_F(HCI_ConnectionTest, AclStartEncryptionSendsSetConnectionEncryptionCommand) {
  auto kExpectedCommand = CreateStaticByteBuffer(0x13, 0x04,  // HCI_Set_Connection_Encryption
                                                 3,           // parameter total size
                                                 0x01, 0x00,  // connection handle
                                                 0x01         // encryption enable
  );
  auto kStatus = CreateStaticByteBuffer(0x0F,       // HCI Command Status event code
                                        4,          // parameter total size
                                        0x00,       // success status
                                        1,          // num_hci_command_packets
                                        0x13, 0x04  // opcode: HCI_Set_Connection_Encryption
  );

  EXPECT_CMD_PACKET_OUT(test_device(), kExpectedCommand, &kStatus);

  bool callback = false;
  auto conn = NewACLConnection();
  conn->set_bredr_link_key(LinkKey(kLTK, 0, 0), kLinkKeyType);
  conn->set_encryption_change_callback([&](Status status, bool enabled) { callback = true; });

  EXPECT_TRUE(conn->StartEncryption());

  // Callback shouldn't be called until the controller sends an encryption changed event.
  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_P(LinkTypeConnectionTest, EncryptionChangeIgnoredEvents) {
  // clang-format off
  auto kEncChangeMalformed = CreateStaticByteBuffer(
    0x08,       // HCI Encryption Change event code
    3,          // parameter total size
    0x00,       // status
    0x01, 0x00  // connection handle: 1
    // Last byte missing
  );
  auto kEncChangeWrongHandle = CreateStaticByteBuffer(
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
  conn->set_encryption_change_callback([&](Status, bool) { callback = true; });

  test_device()->SendCommandChannelPacket(kEncChangeMalformed);
  test_device()->SendCommandChannelPacket(kEncChangeWrongHandle);

  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

const auto kEncryptionChangeEventEnabled =
    CreateStaticByteBuffer(0x08,        // HCI Encryption Change event code
                           4,           // parameter total size
                           0x00,        // status
                           0x01, 0x00,  // connection handle: 1
                           0x01         // encryption enabled
    );

const auto kReadEncryptionKeySizeCommand =
    CreateStaticByteBuffer(0x08, 0x14,  // opcode: HCI_ReadEncryptionKeySize
                           0x02,        // parameter size
                           0x01, 0x00   // connection handle: 0x0001
    );

const auto kDisconnectCommand = CreateStaticByteBuffer(0x06, 0x04,  // opcode: HCI_Disconnect
                                                       0x03,        // parameter total size
                                                       0x01, 0x00,  // handle: 1
                                                       0x05  // reason: authentication failure
);

TEST_P(LinkTypeConnectionTest, EncryptionChangeEvents) {
  // clang-format off
  auto kEncryptionChangeEventDisabled = CreateStaticByteBuffer(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x00,        // status
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );
  auto kEncryptionChangeEventFailed = CreateStaticByteBuffer(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x06,        // status: Pin or Key missing
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );

  auto kKeySizeComplete = CreateStaticByteBuffer(
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

  Status status(HostError::kFailed);
  bool enabled = false;
  conn->set_encryption_change_callback([&](Status cb_status, bool cb_enabled) {
    callback_count++;
    status = cb_status;
    enabled = cb_enabled;
  });

  if (conn->ll_type() == Connection::LinkType::kACL) {
    // The host tries to validate the size of key used to encrypt ACL links.
    EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  }

  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
  EXPECT_TRUE(status);
  EXPECT_TRUE(enabled);

  test_device()->SendCommandChannelPacket(kEncryptionChangeEventDisabled);
  RunLoopUntilIdle();

  EXPECT_EQ(2, callback_count);
  EXPECT_TRUE(status);
  EXPECT_FALSE(enabled);

  // The host should disconnect the link if encryption fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(3, callback_count);
  EXPECT_FALSE(status);
  EXPECT_EQ(StatusCode::kPinOrKeyMissing, status.protocol_error());
}

TEST_F(HCI_ConnectionTest, EncryptionFailureNotifiesPeerDisconnectCallback) {
  bool peer_disconnect_callback_received = false;
  auto conn = NewLEConnection();
  conn->set_peer_disconnect_callback([&](auto* self) {
    EXPECT_EQ(conn.get(), self);
    peer_disconnect_callback_received = true;
  });

  // Send the encryption change failure. The host should disconnect the link as a result.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(testing::EncryptionChangeEventPacket(
      hci::StatusCode::kConnectionTerminatedMICFailure, kTestHandle, EncryptionStatus::kOff));
  RunLoopUntilIdle();
  EXPECT_FALSE(peer_disconnect_callback_received);

  // Send the disconnection complete resulting from the encryption failure (this usually does not
  // correspond to the Disconnect command sent by hci::Connection, which will cause a later
  // subsequent event).
  test_device()->SendCommandChannelPacket(testing::DisconnectionCompletePacket(
      kTestHandle, StatusCode::kConnectionTerminatedMICFailure));
  RunLoopUntilIdle();
  EXPECT_TRUE(peer_disconnect_callback_received);
}

TEST_F(HCI_ConnectionTest, AclEncryptionEnableCanNotReadKeySizeClosesLink) {
  auto kKeySizeComplete = CreateStaticByteBuffer(0x0E,        // event code: Command Complete
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
  conn->set_encryption_change_callback([&callback_count](Status status, bool enabled) {
    callback_count++;
    EXPECT_FALSE(status);
    EXPECT_TRUE(enabled);
  });

  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
}

TEST_F(HCI_ConnectionTest, AclEncryptionEnableKeySizeOneByteClosesLink) {
  auto kKeySizeComplete = CreateStaticByteBuffer(0x0E,        // event code: Command Complete
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
  conn->set_encryption_change_callback([&callback_count](Status status, bool enabled) {
    callback_count++;
    EXPECT_FALSE(status);
    EXPECT_TRUE(enabled);
  });

  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySizeCommand, &kKeySizeComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionChangeEventEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
}

TEST_P(LinkTypeConnectionTest, EncryptionKeyRefreshEvents) {
  // clang-format off
  auto kEncryptionKeyRefresh = CreateStaticByteBuffer(
    0x30,       // HCI Encryption Key Refresh Complete event
    3,          // parameter total size
    0x00,       // status
    0x01, 0x00  // connection handle: 1
  );
  auto kEncryptionKeyRefreshFailed = CreateStaticByteBuffer(
    0x30,       // HCI Encryption Key Refresh Complete event
    3,          // parameter total size
    0x06,       // status: Pin or Key missing
    0x01, 0x00  // connection handle: 1
  );
  // clang-format on

  int callback_count = 0;
  auto conn = NewConnection();

  Status status(HostError::kFailed);
  bool enabled = false;
  conn->set_encryption_change_callback([&](Status cb_status, bool cb_enabled) {
    callback_count++;
    status = cb_status;
    enabled = cb_enabled;
  });

  test_device()->SendCommandChannelPacket(kEncryptionKeyRefresh);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
  EXPECT_TRUE(status);
  EXPECT_TRUE(enabled);

  // The host should disconnect the link if encryption fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnectCommand);
  test_device()->SendCommandChannelPacket(kEncryptionKeyRefreshFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(2, callback_count);
  EXPECT_FALSE(status);
  EXPECT_EQ(StatusCode::kPinOrKeyMissing, status.protocol_error());
  EXPECT_FALSE(enabled);
}

TEST_F(HCI_ConnectionTest, LELongTermKeyRequestIgnoredEvent) {
  // clang-format off
  auto kMalformed = CreateStaticByteBuffer(
    0x3E,        // LE Meta Event code
    12,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 1

    // rand:
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: (missing 1 byte)
    0x00
  );
  auto kWrongHandle = CreateStaticByteBuffer(
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
  conn->set_le_ltk(LinkKey(kLTK, 0, 0));

  test_device()->SendCommandChannelPacket(kMalformed);
  test_device()->SendCommandChannelPacket(kWrongHandle);

  RunLoopUntilIdle();

  // Test will fail if the connection sends a response without ignoring these
  // events.
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kTestHandle));
}

TEST_F(HCI_ConnectionTest, LELongTermKeyRequestNoKey) {
  // clang-format off
  auto kEvent = CreateStaticByteBuffer(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: 0
    0x00, 0x00
  );
  auto kResponse = CreateStaticByteBuffer(
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
TEST_F(HCI_ConnectionTest, LELongTermKeyRequestNoMatchinKey) {
  // clang-format off
  auto kEvent = CreateStaticByteBuffer(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // ediv: 0
    0x00, 0x00
  );
  auto kResponse = CreateStaticByteBuffer(
    0x1B, 0x20,  // opcode: HCI_LE_Long_Term_Key_Request_Negative_Reply
    2,           // parameter total size
    0x01, 0x00   // connection handle: 1
  );
  // clang-format on

  // The request should be rejected since there is no LTK.
  EXPECT_CMD_PACKET_OUT(test_device(), kResponse);
  auto conn = NewLEConnection();
  conn->set_le_ltk(LinkKey(kLTK, 1, 1));

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

TEST_F(HCI_ConnectionTest, LELongTermKeyRequestReply) {
  // clang-format off
  auto kEvent = CreateStaticByteBuffer(
    0x3E,        // LE Meta Event code
    13,          // parameter total size
    0x05,        // LE LTK Request subevent code
    0x01, 0x00,  // connection handle: 2 (wrong)

    // rand: 0x8899AABBCCDDEEFF
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    // ediv: 0xBEEF
    0xEF, 0xBE
  );
  auto kResponse = CreateStaticByteBuffer(
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
  conn->set_le_ltk(LinkKey(kLTK, 0x8899AABBCCDDEEFF, 0xBEEF));

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

TEST_F(HCI_ConnectionTest,
       QueuedPacketsGetDroppedOnDisconnectionCompleteAndStalePacketsAreNotSentOnHandleReuse) {
  const ConnectionHandle kHandle = 0x0001;

  // Should register connection with ACL Data Channel.
  auto conn0 = NewACLConnection(Connection::Role::kMaster, kHandle);

  testing::MockController::DataCallback data_cb = [](const ByteBuffer& packet) {};
  size_t packet_count = 0;
  auto data_cb_wrapper = [&data_cb, &packet_count](const ByteBuffer& packet) {
    packet_count++;
    ASSERT_EQ(packet.size(), sizeof(ACLDataHeader) + 1);
    data_cb(packet);
  };
  test_device()->SetDataCallback(data_cb_wrapper, dispatcher());

  const uint8_t payload0 = 0x01;
  data_cb = [payload0](const ByteBuffer& packet) {
    EXPECT_EQ(packet[sizeof(ACLDataHeader)], payload0);
  };

  // Fill controller buffer, + 1 packet in queue.
  for (size_t i = 0; i < kBrEdrBufferInfo.max_num_packets() + 1; i++) {
    auto packet = ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = payload0;
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
  }

  // Run until the data is flushed out to the MockController.
  RunLoopUntilIdle();

  // Only packets that fit in buffer should have been received.
  EXPECT_EQ(packet_count, kBrEdrBufferInfo.max_num_packets());

  // All future packets received should be for the next connection.
  const uint8_t payload1 = 0x02;
  data_cb = [payload1](const ByteBuffer& packet) {
    EXPECT_EQ(packet[sizeof(ACLDataHeader)], payload1);
  };

  const auto disconnect_status_rsp = testing::DisconnectStatusResponsePacket();
  DynamicByteBuffer disconnection_complete(testing::DisconnectionCompletePacket(kHandle));
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kHandle), &disconnect_status_rsp,
                        &disconnection_complete);

  // Disconnect |conn0| by destroying it. The received disconnection complete event will cause the
  // handler to unregister the link and clear pending packets.
  conn0.reset();
  RunLoopUntilIdle();

  // Register connection with same handle.
  auto conn1 = NewACLConnection(Connection::Role::kMaster, kHandle);

  // Fill controller buffer, + 1 packet in queue.
  for (size_t i = 0; i < kBrEdrBufferInfo.max_num_packets(); i++) {
    auto packet = ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = payload1;
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
  }

  RunLoopUntilIdle();

  EXPECT_EQ(packet_count, 2 * kBrEdrBufferInfo.max_num_packets());

  conn1.reset();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kHandle), &disconnect_status_rsp,
                        &disconnection_complete);
  RunLoopUntilIdle();
}

TEST_F(HCI_ConnectionTest, PeerDisconnectCallback) {
  const ConnectionHandle kHandle = 0x0001;

  auto conn = NewACLConnection(Connection::Role::kMaster, kHandle);

  size_t cb_count = 0;
  auto disconn_complete_cb = [&](const Connection* cb_conn) {
    ASSERT_TRUE(cb_conn);
    cb_count++;

    // Should be safe to destroy connection from this callback, as a connection manager does.
    conn.reset();
  };
  conn->set_peer_disconnect_callback(disconn_complete_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(0u, cb_count);

  DynamicByteBuffer disconnection_complete(testing::DisconnectionCompletePacket(kHandle));
  test_device()->SendCommandChannelPacket(disconnection_complete);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, cb_count);
}

// Test connection handling cases for all types of links.
INSTANTIATE_TEST_SUITE_P(HCI_ConnectionTest, LinkTypeConnectionTest,
                         ::testing::Values(Connection::LinkType::kACL, Connection::LinkType::kLE));

}  // namespace
}  // namespace hci
}  // namespace bt
