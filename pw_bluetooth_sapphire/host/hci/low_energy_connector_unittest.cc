// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"

#include <vector>

#include <lib/async/cpp/task.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_device.h"
#include "lib/fxl/macros.h"

namespace bt {
namespace hci {
namespace {

using bt::testing::FakeController;
using bt::testing::FakeDevice;
using TestingBase = bt::testing::FakeControllerTest<FakeController>;

using common::HostError;
const common::DeviceAddress kLocalAddress(
    common::DeviceAddress::Type::kLEPublic, "00:00:00:00:00:FF");
const common::DeviceAddress kTestAddress(common::DeviceAddress::Type::kLEPublic,
                                         "00:00:00:00:00:01");
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

    connector_ = std::make_unique<LowEnergyConnector>(
        transport(), kLocalAddress, dispatcher(),
        fit::bind_member(this,
                         &LowEnergyConnectorTest::OnIncomingConnectionCreated));

    test_device()->SetConnectionStateCallback(
        fit::bind_member(this,
                         &LowEnergyConnectorTest::OnConnectionStateChanged),
        dispatcher());
    StartTestDevice();
  }

  void TearDown() override {
    connector_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  void DeleteConnector() { connector_ = nullptr; }

  bool request_canceled = false;

  const std::vector<std::unique_ptr<Connection>>& in_connections() const {
    return in_connections_;
  }
  LowEnergyConnector* connector() const { return connector_.get(); }

 private:
  void OnIncomingConnectionCreated(ConnectionHandle handle,
                                   Connection::Role role,
                                   const common::DeviceAddress& peer_address,
                                   const LEConnectionParameters& conn_params) {
    in_connections_.push_back(std::make_unique<testing::FakeConnection>(
        handle, hci::Connection::LinkType::kLE, role, kLocalAddress,
        peer_address));
  }

  void OnConnectionStateChanged(const common::DeviceAddress& address,
                                bool connected, bool canceled) {
    request_canceled = canceled;
  }

  std::unique_ptr<LowEnergyConnector> connector_;

  // Incoming connections.
  std::vector<std::unique_ptr<Connection>> in_connections_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LowEnergyConnectorTest);
};

using HCI_LowEnergyConnectorTest = LowEnergyConnectorTest;

TEST_F(HCI_LowEnergyConnectorTest, CreateConnection) {
  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);
  test_device()->AddDevice(std::move(fake_device));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    conn = std::move(cb_conn);
    callback_called = true;
  };

  bool ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
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
  EXPECT_TRUE(conn->is_open());
  conn->set_closed();
}

// Controller reports error from HCI Command Status event.
TEST_F(HCI_LowEnergyConnectorTest, CreateConnectionStatusError) {
  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);
  fake_device->set_connect_status(StatusCode::kCommandDisallowed);
  test_device()->AddDevice(std::move(fake_device));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    conn = std::move(cb_conn);
    callback_called = true;
  };

  bool ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
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
  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);
  fake_device->set_connect_response(StatusCode::kConnectionRejectedSecurity);
  test_device()->AddDevice(std::move(fake_device));

  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  bool ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
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
  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);

  // Make sure the pending connect remains pending.
  fake_device->set_force_pending_connect(true);
  test_device()->AddDevice(std::move(fake_device));

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  bool ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, callback, kConnectTimeout);
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
                                 common::BufferView(&event, sizeof(event)));

  RunLoopUntilIdle();

  ASSERT_EQ(1u, in_connections().size());

  auto conn = in_connections()[0].get();
  EXPECT_EQ(1u, conn->handle());
  EXPECT_EQ(kLocalAddress, conn->local_address());
  EXPECT_EQ(kTestAddress, conn->peer_address());
  EXPECT_TRUE(conn->is_open());
  conn->set_closed();
}

TEST_F(HCI_LowEnergyConnectorTest, IncomingConnectDuringConnectionRequest) {
  const common::DeviceAddress kIncomingAddress(
      common::DeviceAddress::Type::kLEPublic, "00:00:00:00:00:02");

  EXPECT_TRUE(in_connections().empty());
  EXPECT_FALSE(connector()->request_pending());

  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);
  test_device()->AddDevice(std::move(fake_device));

  hci::Status status;
  ConnectionPtr conn;
  unsigned int callback_count = 0;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_count++;
    conn = std::move(cb_conn);
  };

  connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
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
                                   common::BufferView(&event, sizeof(event)));
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

  EXPECT_TRUE(conn->is_open());
  EXPECT_TRUE(in_conn->is_open());
  conn->set_closed();
  in_conn->set_closed();
}

TEST_F(HCI_LowEnergyConnectorTest, CreateConnectionTimeout) {
  // We do not set up any fake devices. This will cause the request to time out.
  EXPECT_FALSE(connector()->request_pending());

  hci::Status status;
  ConnectionPtr conn;
  bool callback_called = false;

  auto callback = [&, this](auto cb_status, auto cb_conn) {
    status = cb_status;
    callback_called = true;
    conn = std::move(cb_conn);
  };

  connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
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
  auto fake_device = std::make_unique<FakeDevice>(kTestAddress, true, true);

  // Make sure the pending connect remains pending.
  fake_device->set_force_pending_connect(true);
  test_device()->AddDevice(std::move(fake_device));

  bool ret = connector()->CreateConnection(
      LEOwnAddressType::kPublic, false, kTestAddress, defaults::kLEScanInterval,
      defaults::kLEScanWindow, kTestParams, [](auto, auto) {}, kConnectTimeout);
  EXPECT_TRUE(ret);
  EXPECT_TRUE(connector()->request_pending());

  DeleteConnector();
  RunLoopUntilIdle();

  EXPECT_TRUE(request_canceled);
  EXPECT_TRUE(in_connections().empty());
}

}  // namespace
}  // namespace hci
}  // namespace bt
