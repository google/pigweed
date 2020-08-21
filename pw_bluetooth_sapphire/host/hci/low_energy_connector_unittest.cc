// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"

#include <lib/async/cpp/task.h>

#include <vector>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {
namespace hci {
namespace {

using bt::testing::FakeController;
using bt::testing::FakePeer;
using TestingBase = bt::testing::ControllerTest<FakeController>;

const DeviceAddress kLocalAddress(DeviceAddress::Type::kLEPublic, {0xFF});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {0xFE});
const DeviceAddress kTestAddress(DeviceAddress::Type::kLEPublic, {1});
const LEPreferredConnectionParameters kTestParams(1, 1, 1, 1);
constexpr zx::duration kConnectTimeout = zx::sec(10);

class LowEnergyConnectorTest : public TestingBase {
 public:
  LowEnergyConnectorTest() = default;
  ~LowEnergyConnectorTest() override = default;

 protected:
  // TestingBase overrides:
  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    fake_address_delegate_.set_local_address(kLocalAddress);
    connector_ = std::make_unique<LowEnergyConnector>(
        transport()->WeakPtr(), &fake_address_delegate_, dispatcher(),
        fit::bind_member(this, &LowEnergyConnectorTest::OnIncomingConnectionCreated));

    test_device()->set_connection_state_callback(
        fit::bind_member(this, &LowEnergyConnectorTest::OnConnectionStateChanged));
    StartTestDevice();
  }

  void TearDown() override {
    connector_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  void DeleteConnector() { connector_ = nullptr; }

  bool request_canceled = false;

  const std::vector<std::unique_ptr<Connection>>& in_connections() const { return in_connections_; }
  LowEnergyConnector* connector() const { return connector_.get(); }
  FakeLocalAddressDelegate* fake_address_delegate() { return &fake_address_delegate_; }

 private:
  void OnIncomingConnectionCreated(ConnectionHandle handle, Connection::Role role,
                                   const DeviceAddress& peer_address,
                                   const LEConnectionParameters& conn_params) {
    in_connections_.push_back(std::make_unique<testing::FakeConnection>(
        handle, hci::Connection::LinkType::kLE, role, kLocalAddress, peer_address));
  }

  void OnConnectionStateChanged(const DeviceAddress& address, hci::ConnectionHandle handle,
                                bool connected, bool canceled) {
    request_canceled = canceled;
  }

  FakeLocalAddressDelegate fake_address_delegate_;
  std::unique_ptr<LowEnergyConnector> connector_;

  // Incoming connections.
  std::vector<std::unique_ptr<Connection>> in_connections_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectorTest);
};

using HCI_LowEnergyConnectorTest = LowEnergyConnectorTest;

TEST_F(HCI_LowEnergyConnectorTest, CreateConnection) {
  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);
  test_device()->AddPeer(std::move(fake_peer));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    conn = std::move(cb_conn);
    callback_called = true;
  };

  bool ret = connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                           defaults::kLEScanWindow, kTestParams, callback,
                                           kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  ret = connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                      defaults::kLEScanWindow, kTestParams, callback,
                                      kConnectTimeout);
  EXPECT_FALSE(ret);

  RunLoopUntilIdle();

  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(status);
  EXPECT_TRUE(in_connections().empty());

  ASSERT_TRUE(conn);
  EXPECT_EQ(1u, conn->handle());
  EXPECT_EQ(kLocalAddress, conn->local_address());
  EXPECT_EQ(kTestAddress, conn->peer_address());
  conn->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
}

// Controller reports error from HCI Command Status event.
TEST_F(HCI_LowEnergyConnectorTest, CreateConnectionStatusError) {
  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);
  fake_peer->set_connect_status(StatusCode::kCommandDisallowed);
  test_device()->AddPeer(std::move(fake_peer));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    conn = std::move(cb_conn);
    callback_called = true;
  };

  bool ret = connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                           defaults::kLEScanWindow, kTestParams, callback,
                                           kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  RunLoopUntilIdle();

  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(StatusCode::kCommandDisallowed, status.protocol_error());
  EXPECT_FALSE(conn);
  EXPECT_TRUE(in_connections().empty());
}

// Controller reports error from HCI LE Connection Complete event
TEST_F(HCI_LowEnergyConnectorTest, CreateConnectionEventError) {
  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);
  fake_peer->set_connect_response(StatusCode::kConnectionRejectedSecurity);
  test_device()->AddPeer(std::move(fake_peer));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  bool ret = connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                           defaults::kLEScanWindow, kTestParams, callback,
                                           kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  RunLoopUntilIdle();

  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(StatusCode::kConnectionRejectedSecurity, status.protocol_error());
  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(conn);
}

// Controller reports error from HCI LE Connection Complete event
TEST_F(HCI_LowEnergyConnectorTest, Cancel) {
  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);

  // Make sure the pending connect remains pending.
  fake_peer->set_force_pending_connect(true);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  bool ret = connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                           defaults::kLEScanWindow, kTestParams, callback,
                                           kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  ASSERT_FALSE(request_canceled);

  connector()->Cancel();
  EXPECT_TRUE(connector()->request_pending());

  // The request timeout should be canceled regardless of whether it was posted
  // before.
  EXPECT_FALSE(connector()->timeout_posted());

  RunLoopUntilIdle();

  EXPECT_FALSE(connector()->timeout_posted());
  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_canceled);
  EXPECT_EQ(HostError::kCanceled, status.error());
  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(conn);
}

TEST_F(HCI_LowEnergyConnectorTest, IncomingConnect) {
  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(connector()->request_pending());

  LEConnectionCompleteSubeventParams event;
  std::memset(&event, 0, sizeof(event));

  event.status = StatusCode::kSuccess;
  event.peer_address = kTestAddress.value();
  event.peer_address_type = LEPeerAddressType::kPublic;
  event.conn_interval = defaults::kLEConnectionIntervalMin;
  event.connection_handle = 1;

  test_device()->SendLEMetaEvent(kLEConnectionCompleteSubeventCode,
                                 BufferView(&event, sizeof(event)));

  RunLoopUntilIdle();

  ASSERT_EQ(1u, in_connections().size());

  auto conn = in_connections()[0].get();
  EXPECT_EQ(1u, conn->handle());
  EXPECT_EQ(kLocalAddress, conn->local_address());
  EXPECT_EQ(kTestAddress, conn->peer_address());
  conn->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
}

TEST_F(HCI_LowEnergyConnectorTest, IncomingConnectDuringConnectionRequest) {
  const DeviceAddress kIncomingAddress(DeviceAddress::Type::kLEPublic, {2});

  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(connector()->request_pending());

  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status status;
  ConnectionPtr conn;
  unsigned int callback_count = 0;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_count++;
    conn = std::move(cb_conn);
  };

  connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);

  async::PostTask(dispatcher(), [kIncomingAddress, this] {
    LEConnectionCompleteSubeventParams event;
    std::memset(&event, 0, sizeof(event));

    event.status = StatusCode::kSuccess;
    event.peer_address = kIncomingAddress.value();
    event.peer_address_type = LEPeerAddressType::kPublic;
    event.conn_interval = defaults::kLEConnectionIntervalMin;
    event.connection_handle = 2;

    test_device()->SendLEMetaEvent(kLEConnectionCompleteSubeventCode,
                                   BufferView(&event, sizeof(event)));
  });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, callback_count);
  ASSERT_EQ(1u, in_connections().size());

  const auto& in_conn = in_connections().front();

  EXPECT_EQ(1u, conn->handle());
  EXPECT_EQ(2u, in_conn->handle());
  EXPECT_EQ(kTestAddress, conn->peer_address());
  EXPECT_EQ(kIncomingAddress, in_conn->peer_address());

  conn->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
  in_conn->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
}

TEST_F(HCI_LowEnergyConnectorTest, CreateConnectionTimeout) {
  // We do not set up any fake devices. This will cause the request to time out.
  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
  EXPECT_TRUE(connector()->request_pending());

  EXPECT_FALSE(request_canceled);

  // Make the connection attempt time out.
  RunLoopFor(kConnectTimeout);

  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(request_canceled);
  EXPECT_EQ(HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(conn);
}

TEST_F(HCI_LowEnergyConnectorTest, SendRequestAndDelete) {
  auto fake_peer = std::make_unique<FakePeer>(kTestAddress, true, true);

  // Make sure the pending connect remains pending.
  fake_peer->set_force_pending_connect(true);
  test_device()->AddPeer(std::move(fake_peer));

  bool ret = connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  DeleteConnector();
  RunLoopUntilIdle();

  EXPECT_TRUE(request_canceled);
  EXPECT_TRUE(in_connections().empty());
}

TEST_F(HCI_LowEnergyConnectorTest, AllowsRandomAddressChange) {
  EXPECT_TRUE(connector()->AllowsRandomAddressChange());

  auto fake_device = std::make_unique<FakePeer>(kTestAddress, true, true);
  test_device()->AddPeer(std::move(fake_device));

  // Address change should not be allowed while the procedure is pending.
  connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  EXPECT_TRUE(connector()->request_pending());
  EXPECT_FALSE(connector()->AllowsRandomAddressChange());

  RunLoopUntilIdle();
  EXPECT_TRUE(connector()->AllowsRandomAddressChange());
}

TEST_F(HCI_LowEnergyConnectorTest, AllowsRandomAddressChangeWhileRequestingLocalAddress) {
  // Make the local address delegate report its result asynchronously.
  fake_address_delegate()->set_async(true);

  // The connector should be in the "request pending" state without initiating
  // controller procedures that would prevent a local address change.
  connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  EXPECT_TRUE(connector()->request_pending());
  EXPECT_TRUE(connector()->AllowsRandomAddressChange());

  // Initiating a new connection should fail in this state.
  bool result = connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  EXPECT_FALSE(result);

  // After the loop runs the request should remain pending (since we added no
  // fake device, the request would eventually timeout) but address change
  // should no longer be allowed.
  RunLoopUntilIdle();
  EXPECT_TRUE(connector()->request_pending());
  EXPECT_FALSE(connector()->AllowsRandomAddressChange());
}

TEST_F(HCI_LowEnergyConnectorTest, ConnectUsingPublicAddress) {
  auto fake_device = std::make_unique<FakePeer>(kTestAddress, true, true);
  test_device()->AddPeer(std::move(fake_device));
  connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_connect_params());
  EXPECT_EQ(LEOwnAddressType::kPublic, test_device()->le_connect_params()->own_address_type);
}

TEST_F(HCI_LowEnergyConnectorTest, ConnectUsingRandomAddress) {
  fake_address_delegate()->set_local_address(kRandomAddress);

  auto fake_device = std::make_unique<FakePeer>(kTestAddress, true, true);
  test_device()->AddPeer(std::move(fake_device));
  connector()->CreateConnection(
      false, kTestAddress, defaults::kLEScanInterval, defaults::kLEScanWindow, kTestParams,
      [](auto, auto) {}, kConnectTimeout);
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_connect_params());
  EXPECT_EQ(LEOwnAddressType::kRandom, test_device()->le_connect_params()->own_address_type);
}

TEST_F(HCI_LowEnergyConnectorTest, CancelConnectWhileWaitingForLocalAddress) {
  Status status;
  ConnectionPtr conn;
  auto callback = [&](auto s, auto c) {
    status = s;
    conn = std::move(c);
  };
  fake_address_delegate()->set_async(true);
  connector()->CreateConnection(false, kTestAddress, defaults::kLEScanInterval,
                                defaults::kLEScanWindow, kTestParams, std::move(callback),
                                kConnectTimeout);

  // Should be waiting for the address.
  EXPECT_TRUE(connector()->request_pending());
  EXPECT_TRUE(connector()->AllowsRandomAddressChange());

  connector()->Cancel();
  RunLoopUntilIdle();
  EXPECT_FALSE(connector()->request_pending());
  EXPECT_TRUE(connector()->AllowsRandomAddressChange());

  // The controller should have received no command from us.
  EXPECT_FALSE(test_device()->le_connect_params());
  EXPECT_FALSE(request_canceled);

  // Our request should have resulted in an error.
  EXPECT_EQ(HostError::kCanceled, status.error());
  EXPECT_FALSE(conn);
}

}  // namespace
}  // namespace hci
}  // namespace bt
