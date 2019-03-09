// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace btlib {
namespace hci {
namespace {

using common::CreateStaticByteBuffer;
using common::DeviceAddress;
using common::HostError;
using common::UInt128;

constexpr ConnectionHandle kTestHandle = 0x0001;
const LEConnectionParameters kTestParams(1, 1, 1);
const DeviceAddress kLEAddress1(DeviceAddress::Type::kLEPublic,
                                "00:00:00:00:00:01");
const DeviceAddress kLEAddress2(DeviceAddress::Type::kLEPublic,
                                "00:00:00:00:00:02");
const DeviceAddress kACLAddress1(DeviceAddress::Type::kBREDR,
                                 "00:00:00:00:00:03");
const DeviceAddress kACLAddress2(DeviceAddress::Type::kBREDR,
                                 "00:00:00:00:00:04");

constexpr UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
constexpr uint64_t kRand = 1;
constexpr uint16_t kEDiv = 255;

const DataBufferInfo kBrEdrBufferInfo(1024, 5);
const DataBufferInfo kLeBufferInfo(1024, 1);

using ::btlib::testing::CommandTransaction;

using TestingBase =
    ::btlib::testing::FakeControllerTest<::btlib::testing::TestController>;

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

  ConnectionPtr NewLEConnection(
      Connection::Role role = Connection::Role::kMaster) {
    return Connection::CreateLE(kTestHandle, role, kLEAddress1, kLEAddress2,
                                kTestParams, transport());
  }

  ConnectionPtr NewACLConnection(
      Connection::Role role = Connection::Role::kMaster) {
    return Connection::CreateACL(kTestHandle, role, kACLAddress1, kACLAddress2,
                                 transport());
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
  EXPECT_TRUE(connection->is_open());
}

TEST_F(HCI_ConnectionTest, Close) {
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

  test_device()->QueueCommandTransaction(req_bytes,
                                         {&cmd_status_bytes, &disc_cmpl_bytes});

  bool callback_called = false;
  test_device()->SetTransactionCallback(
      [&callback_called, this] { callback_called = true; }, dispatcher());

  auto connection = NewLEConnection();
  EXPECT_TRUE(connection->is_open());

  connection->Close(StatusCode::kRemoteUserTerminatedConnection);
  EXPECT_FALSE(connection->is_open());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HCI_ConnectionTest, CloseError) {
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

  test_device()->QueueCommandTransaction(req_bytes,
                                         {&cmd_status_bytes, &disc_cmpl_bytes});

  // The callback should get called regardless of the procedure status.
  bool callback_called = false;
  test_device()->SetTransactionCallback(
      [&callback_called, this] { callback_called = true; }, dispatcher());

  auto connection = NewLEConnection();
  EXPECT_TRUE(connection->is_open());

  connection->Close(StatusCode::kRemoteUserTerminatedConnection);
  EXPECT_FALSE(connection->is_open());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(HCI_ConnectionTest, StartEncryptionFailsAsSlave) {
  auto conn = NewLEConnection(Connection::Role::kSlave);
  conn->set_link_key(LinkKey());
  EXPECT_FALSE(conn->StartEncryption());
}

// TODO(armansito): This test is temporary to document the current state of
// support for link layer encryption. Remove this and replace it with new tests
// when StartEncryption can work with BR/EDR.
TEST_F(HCI_ConnectionTest, StartEncryptionNotSupportedOnACL) {
  auto conn = NewACLConnection();
  conn->set_link_key(LinkKey());
  EXPECT_FALSE(conn->StartEncryption());
}

TEST_F(HCI_ConnectionTest, StartEncryptionNoLinkKey) {
  auto conn = NewLEConnection();
  EXPECT_FALSE(conn->StartEncryption());
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

  test_device()->QueueCommandTransaction(kExpectedCommand, {&kErrorStatus});

  bool callback = false;
  auto conn = NewLEConnection();
  conn->set_link_key(LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](Status status, bool enabled) {
    EXPECT_FALSE(status);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(StatusCode::kCommandDisallowed, status.protocol_error());
    callback = true;
  });

  EXPECT_TRUE(conn->StartEncryption());

  RunLoopUntilIdle();
  EXPECT_TRUE(callback);
}

TEST_F(HCI_ConnectionTest, LEStartEncryptionSuccess) {
  auto kExpectedCommand = CreateStaticByteBuffer(
      0x19, 0x20,  // HCI_LE_Start_Encryption
      28,          // parameter total size
      0x01, 0x00,  // connection handle: 1
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,        // rand: 1
      0xFF, 0x00,                                            // ediv: 255
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16  // LTK
  );
  auto kStatus =
      CreateStaticByteBuffer(0x0F,       // HCI Command Status event code
                             4,          // parameter total size
                             0x00,       // success status
                             1,          // num_hci_command_packets
                             0x19, 0x20  // opcode: HCI_LE_Start_Encryption
      );

  test_device()->QueueCommandTransaction(kExpectedCommand, {&kStatus});

  bool callback = false;
  auto conn = NewLEConnection();
  conn->set_link_key(LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback(
      [&](Status status, bool enabled) { callback = true; });

  EXPECT_TRUE(conn->StartEncryption());

  // Callback shouldn't be called until the controller sends an encryption
  // changed event.
  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
}

TEST_F(HCI_ConnectionTest, EncryptionChangeIgnoredEvents) {
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
  auto conn = NewLEConnection();
  conn->set_link_key(LinkKey(kLTK, kRand, kEDiv));
  conn->set_encryption_change_callback([&](Status, bool) { callback = true; });

  test_device()->SendCommandChannelPacket(kEncChangeMalformed);
  test_device()->SendCommandChannelPacket(kEncChangeWrongHandle);

  RunLoopUntilIdle();
  EXPECT_FALSE(callback);
}

TEST_F(HCI_ConnectionTest, EncryptionChangeEvents) {
  // clang-format off
  auto kEncryptionEnabled = CreateStaticByteBuffer(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x00,        // status
    0x01, 0x00,  // connection handle: 1
    0x01         // encryption enabled
  );
  auto kEncryptionDisabled = CreateStaticByteBuffer(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x00,        // status
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );
  auto kEncryptionFailed = CreateStaticByteBuffer(
    0x08,        // HCI Encryption Change event code
    4,           // parameter total size
    0x06,        // status: Pin or Key missing
    0x01, 0x00,  // connection handle: 1
    0x00         // encryption disabled
  );

  auto kDisconnect = CreateStaticByteBuffer(
    0x06, 0x04,  // opcode: HCI_Disconnect
    0x03,        // parameter total size
    0x01, 0x00,  // handle: 1
    0x05         // reason: authentication failure
  );
  // clang-format on

  int callback_count = 0;
  auto conn = NewLEConnection();

  Status status(HostError::kFailed);
  bool enabled = false;
  conn->set_encryption_change_callback([&](Status cb_status, bool cb_enabled) {
    callback_count++;
    status = cb_status;
    enabled = cb_enabled;
  });

  test_device()->SendCommandChannelPacket(kEncryptionEnabled);
  RunLoopUntilIdle();

  EXPECT_EQ(1, callback_count);
  EXPECT_TRUE(status);
  EXPECT_TRUE(enabled);

  test_device()->SendCommandChannelPacket(kEncryptionDisabled);
  RunLoopUntilIdle();

  EXPECT_EQ(2, callback_count);
  EXPECT_TRUE(status);
  EXPECT_FALSE(enabled);

  // The host should disconnect the link if encryption fails.
  test_device()->QueueCommandTransaction(kDisconnect, {});
  test_device()->SendCommandChannelPacket(kEncryptionFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(3, callback_count);
  EXPECT_FALSE(status);
  EXPECT_EQ(StatusCode::kPinOrKeyMissing, status.protocol_error());
}

TEST_F(HCI_ConnectionTest, EncryptionKeyRefreshEvents) {
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

  auto kDisconnect = CreateStaticByteBuffer(
    0x06, 0x04,  // opcode: HCI_Disconnect
    0x03,        // parameter total size
    0x01, 0x00,  // handle: 1
    0x05         // reason: authentication failure
  );
  // clang-format on

  int callback_count = 0;
  auto conn = NewLEConnection();

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
  test_device()->QueueCommandTransaction(kDisconnect, {});
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
  conn->set_link_key(LinkKey(kLTK, 0, 0));

  test_device()->SendCommandChannelPacket(kMalformed);
  test_device()->SendCommandChannelPacket(kWrongHandle);

  RunLoopUntilIdle();

  // Test will fail if the connection sends a response without ignoring these
  // events.
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
  test_device()->QueueCommandTransaction(kResponse, {});
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
  test_device()->QueueCommandTransaction(kResponse, {});
  auto conn = NewLEConnection();
  conn->set_link_key(LinkKey(kLTK, 1, 1));

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
  test_device()->QueueCommandTransaction(kResponse, {});
  auto conn = NewLEConnection();
  conn->set_link_key(LinkKey(kLTK, 0x8899AABBCCDDEEFF, 0xBEEF));

  test_device()->SendCommandChannelPacket(kEvent);
  RunLoopUntilIdle();
}

// Tests that a Connection clears the ACL data channel state associated with its
// connection handle during destruction.
TEST_F(HCI_ConnectionTest, ClearAclState) {
  constexpr size_t kMaxNumPackets = 1;
  ASSERT_EQ(kMaxNumPackets, kLeBufferInfo.max_num_packets());

  auto conn = NewLEConnection();

  // TODO(NET-1211): Change this test to exercise enable/disable functionality.
  // Consider creating a FakeAclDataChannel class to prevent duplicating tests
  // in HCI_ConnectionTest and HCI_ACLDataChannelTest.
  size_t packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; },
                                 dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(conn->handle(),
                         ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      Connection::LinkType::kLE));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(conn->handle(),
                         ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      Connection::LinkType::kLE));

  RunLoopUntilIdle();

  // The second packet should have been queued.
  ASSERT_EQ(kMaxNumPackets, packet_count);

  // Mark the connection as closed so that destroying it doesn't send
  // HCI_Disconnect. ACLDataChannel should get updated regardless.
  conn->set_closed();

  // This should allow the next packet to go out.
  conn = nullptr;
  RunLoopUntilIdle();
  ASSERT_EQ(2u, packet_count);
}

}  // namespace
}  // namespace hci
}  // namespace btlib
