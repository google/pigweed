// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/fidl/low_energy_peripheral_server.h"

#include "adapter_test_fixture.h"
#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/le/cpp/fidl.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bthost {
namespace {

namespace fble = fuchsia::bluetooth::le;
const bt::DeviceAddress kTestAddr(bt::DeviceAddress::Type::kLEPublic, {0x01, 0, 0, 0, 0, 0});
const bt::DeviceAddress kTestAddr2(bt::DeviceAddress::Type::kLEPublic, {0x02, 0, 0, 0, 0, 0});

using bt::testing::FakePeer;
using FidlAdvHandle = fidl::InterfaceHandle<fble::AdvertisingHandle>;

class FIDL_LowEnergyPeripheralServerTest_FakeAdapter
    : public bt::gap::testing::FakeAdapterTestFixture {
 public:
  FIDL_LowEnergyPeripheralServerTest_FakeAdapter() = default;
  ~FIDL_LowEnergyPeripheralServerTest_FakeAdapter() override = default;

  void SetUp() override {
    bt::gap::testing::FakeAdapterTestFixture::SetUp();

    // Create a LowEnergyPeripheralServer and bind it to a local client.
    fidl::InterfaceHandle<fble::Peripheral> handle;
    server_ =
        std::make_unique<LowEnergyPeripheralServer>(adapter()->AsWeakPtr(), handle.NewRequest());
    peripheral_client_.Bind(std::move(handle));
  }

  void TearDown() override {
    RunLoopUntilIdle();

    peripheral_client_ = nullptr;
    server_ = nullptr;
    bt::gap::testing::FakeAdapterTestFixture::TearDown();
  }

  LowEnergyPeripheralServer* server() const { return server_.get(); }

  void SetOnPeerConnectedCallback(fble::Peripheral::OnPeerConnectedCallback cb) {
    peripheral_client_.events().OnPeerConnected = std::move(cb);
  }

 private:
  std::unique_ptr<LowEnergyPeripheralServer> server_;
  fble::PeripheralPtr peripheral_client_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_LowEnergyPeripheralServerTest_FakeAdapter);
};

class FIDL_LowEnergyPeripheralServerTest : public bthost::testing::AdapterTestFixture {
 public:
  FIDL_LowEnergyPeripheralServerTest() = default;
  ~FIDL_LowEnergyPeripheralServerTest() override = default;

  void SetUp() override {
    AdapterTestFixture::SetUp();

    // Create a LowEnergyPeripheralServer and bind it to a local client.
    fidl::InterfaceHandle<fble::Peripheral> handle;
    server_ = std::make_unique<LowEnergyPeripheralServer>(adapter(), handle.NewRequest());
    peripheral_client_.Bind(std::move(handle));
  }

  void TearDown() override {
    RunLoopUntilIdle();

    peripheral_client_ = nullptr;
    server_ = nullptr;
    AdapterTestFixture::TearDown();
  }

  LowEnergyPeripheralServer* server() const { return server_.get(); }

  void SetOnPeerConnectedCallback(fble::Peripheral::OnPeerConnectedCallback cb) {
    peripheral_client_.events().OnPeerConnected = std::move(cb);
  }

 private:
  std::unique_ptr<LowEnergyPeripheralServer> server_;
  fble::PeripheralPtr peripheral_client_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_LowEnergyPeripheralServerTest);
};

class BoolParam : public FIDL_LowEnergyPeripheralServerTest,
                  public ::testing::WithParamInterface<bool> {};

class FakeAdvertisedPeripheral : public ServerBase<fble::AdvertisedPeripheral> {
 public:
  struct Connection {
    fble::Peer peer;
    fble::ConnectionHandle connection;
    OnConnectedCallback callback;
  };

  FakeAdvertisedPeripheral(fidl::InterfaceRequest<fble::AdvertisedPeripheral> request)
      : ServerBase(this, std::move(request)) {}

  void Unbind() { binding()->Unbind(); }

  void OnConnected(fble::Peer peer, fidl::InterfaceHandle<fble::Connection> connection,
                   OnConnectedCallback callback) override {
    connections_.push_back({std::move(peer), std::move(connection), std::move(callback)});
  }

  std::optional<bt::PeerId> last_connected_peer() const {
    if (connections_.empty()) {
      return std::nullopt;
    }
    return bt::PeerId(connections_.back().peer.id().value);
  }

  std::vector<Connection>& connections() { return connections_; }

 private:
  std::vector<Connection> connections_;
};

// Tests that aborting a StartAdvertising command sequence does not cause a crash in successive
// requests.
TEST_F(FIDL_LowEnergyPeripheralServerTest, StartAdvertisingWhilePendingDoesNotCrash) {
  fble::AdvertisingParameters params1, params2, params3;
  FidlAdvHandle token1, token2, token3;

  std::optional<fpromise::result<void, fble::PeripheralError>> result1, result2, result3;
  server()->StartAdvertising(std::move(params1), token1.NewRequest(),
                             [&](auto result) { result1 = std::move(result); });
  server()->StartAdvertising(std::move(params2), token2.NewRequest(),
                             [&](auto result) { result2 = std::move(result); });
  server()->StartAdvertising(std::move(params3), token3.NewRequest(),
                             [&](auto result) { result3 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result3);
  EXPECT_TRUE(result1->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result1->error());
  EXPECT_TRUE(result2->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result2->error());
  EXPECT_TRUE(result3->is_ok());
}

// Same as the test above but tests that an error status leaves the server in the expected state.
TEST_F(FIDL_LowEnergyPeripheralServerTest,
       StartAdvertisingWhilePendingDoesNotCrashWithControllerError) {
  test_device()->SetDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable,
                                          bt::hci::StatusCode::kCommandDisallowed);
  fble::AdvertisingParameters params1, params2, params3, params4;
  FidlAdvHandle token1, token2, token3, token4;

  std::optional<fpromise::result<void, fble::PeripheralError>> result1, result2, result3, result4;
  server()->StartAdvertising(std::move(params1), token1.NewRequest(),
                             [&](auto result) { result1 = std::move(result); });
  server()->StartAdvertising(std::move(params2), token2.NewRequest(),
                             [&](auto result) { result2 = std::move(result); });
  server()->StartAdvertising(std::move(params3), token3.NewRequest(),
                             [&](auto result) { result3 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result3);
  EXPECT_TRUE(result1->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result1->error());
  EXPECT_TRUE(result2->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result2->error());
  EXPECT_TRUE(result3->is_error());
  EXPECT_EQ(fble::PeripheralError::FAILED, result3->error());

  // The next request should succeed as normal.
  test_device()->ClearDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable);
  server()->StartAdvertising(std::move(params4), token4.NewRequest(),
                             [&](auto result) { result4 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result4);
  EXPECT_TRUE(result4->is_ok());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseWhilePendingDoesNotCrashWithControllerError) {
  test_device()->SetDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable,
                                          bt::hci::StatusCode::kCommandDisallowed);
  fble::AdvertisingParameters params1, params2, params3, params4;

  fble::AdvertisedPeripheralHandle adv_peripheral_handle_1;
  FakeAdvertisedPeripheral adv_peripheral_server_1(adv_peripheral_handle_1.NewRequest());
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_2;
  FakeAdvertisedPeripheral adv_peripheral_server_2(adv_peripheral_handle_2.NewRequest());
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_3;
  FakeAdvertisedPeripheral adv_peripheral_server_3(adv_peripheral_handle_3.NewRequest());

  std::optional<fpromise::result<void, fble::PeripheralError>> result1, result2, result3, result4;
  server()->Advertise(std::move(params1), std::move(adv_peripheral_handle_1),
                      [&](auto result) { result1 = std::move(result); });
  server()->Advertise(std::move(params2), std::move(adv_peripheral_handle_2),
                      [&](auto result) { result2 = std::move(result); });
  server()->Advertise(std::move(params3), std::move(adv_peripheral_handle_3),
                      [&](auto result) { result3 = std::move(result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result3);
  EXPECT_TRUE(result1->is_error());
  EXPECT_EQ(fble::PeripheralError::FAILED, result1->error());
  EXPECT_TRUE(result2->is_error());
  EXPECT_EQ(fble::PeripheralError::FAILED, result2->error());
  EXPECT_TRUE(result3->is_error());
  EXPECT_EQ(fble::PeripheralError::FAILED, result3->error());

  // The next request should succeed as normal.
  test_device()->ClearDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable);

  fble::AdvertisedPeripheralHandle adv_peripheral_handle_4;
  FakeAdvertisedPeripheral adv_peripheral_server_4(adv_peripheral_handle_4.NewRequest());
  server()->Advertise(std::move(params4), std::move(adv_peripheral_handle_4),
                      [&](auto result) { result4 = std::move(result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(result4);
  adv_peripheral_server_4.Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(result4);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, StartAdvertisingNoConnectionRelatedParamsNoConnection) {
  fble::Peer peer;
  // `conn` is stored so the bondable mode of the connection resulting from `OnPeerConnected` can
  // be checked. The connection would otherwise be dropped immediately after `ConnectLowEnergy`.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  fble::AdvertisingParameters params;

  FidlAdvHandle token;

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->StartAdvertising(std::move(params), token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result->is_error());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();

  ASSERT_FALSE(peer.has_id());
  ASSERT_FALSE(conn.is_valid());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseNoConnectionRelatedParamsNoConnection) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());
  fble::AdvertisingParameters params;
  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_peripheral_server.last_connected_peer());
  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(result);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest,
       StartAdvertisingConnectableParameterTrueConnectsBondable) {
  fble::Peer peer;
  // `conn` is stored so the bondable mode of the connection resulting from `OnPeerConnected` can
  // be checked. The connection would otherwise be dropped immediately after `ConnectLowEnergy`.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  fble::AdvertisingParameters params;
  params.set_connectable(true);

  FidlAdvHandle token;

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->StartAdvertising(std::move(params), token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result->is_error());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();

  ASSERT_TRUE(peer.has_id());
  ASSERT_TRUE(conn.is_valid());

  auto connected_id = bt::PeerId(peer.id().value);
  const bt::gap::LowEnergyConnectionHandle* conn_handle =
      server()->FindConnectionForTesting(connected_id);

  ASSERT_TRUE(conn_handle);
  ASSERT_EQ(conn_handle->bondable_mode(), bt::sm::BondableMode::Bondable);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, StartAdvertisingEmptyConnectionOptionsConnectsBondable) {
  fble::Peer peer;
  // `conn` is stored so the bondable mode of the connection resulting from `OnPeerConnected` can
  // be checked. The connection would otherwise be dropped immediately after `ConnectLowEnergy`.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  params.set_connection_options(std::move(conn_opts));

  FidlAdvHandle token;

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->StartAdvertising(std::move(params), token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result->is_error());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();

  ASSERT_TRUE(peer.has_id());
  ASSERT_TRUE(conn.is_valid());

  auto connected_id = bt::PeerId(peer.id().value);
  const bt::gap::LowEnergyConnectionHandle* conn_handle =
      server()->FindConnectionForTesting(connected_id);

  ASSERT_TRUE(conn_handle);
  ASSERT_EQ(conn_handle->bondable_mode(), bt::sm::BondableMode::Bondable);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseEmptyConnectionOptionsConnectsBondable) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  params.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  auto connected_id = adv_peripheral_server.last_connected_peer();
  ASSERT_TRUE(connected_id);

  const bt::gap::LowEnergyConnectionHandle* conn_handle =
      server()->FindConnectionForTesting(*connected_id);
  ASSERT_TRUE(conn_handle);
  ASSERT_EQ(conn_handle->bondable_mode(), bt::sm::BondableMode::Bondable);

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(result.has_value());
}

TEST_P(BoolParam, AdvertiseBondableOrNonBondableConnectsBondableOrNonBondable) {
  const bool bondable = GetParam();

  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  conn_opts.set_bondable_mode(bondable);
  params.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  auto connected_id = adv_peripheral_server.last_connected_peer();
  ASSERT_TRUE(connected_id);

  const bt::gap::LowEnergyConnectionHandle* conn_handle =
      server()->FindConnectionForTesting(*connected_id);
  ASSERT_TRUE(conn_handle);
  EXPECT_EQ(conn_handle->bondable_mode(),
            bondable ? bt::sm::BondableMode::Bondable : bt::sm::BondableMode::NonBondable);

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
}

TEST_P(BoolParam, StartAdvertisingBondableOrNonBondableConnectsBondableOrNonBondable) {
  const bool bondable = GetParam();

  fble::Peer peer;
  // `conn` is stored so the bondable mode of the connection resulting from `OnPeerConnected` can
  // be checked. The connection would otherwise be dropped immediately after `ConnectLowEnergy`.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  conn_opts.set_bondable_mode(bondable);
  params.set_connection_options(std::move(conn_opts));

  FidlAdvHandle token;

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->StartAdvertising(std::move(params), token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->is_error());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();

  ASSERT_TRUE(peer.has_id());
  ASSERT_TRUE(conn.is_valid());

  auto connected_id = bt::PeerId(peer.id().value);
  const bt::gap::LowEnergyConnectionHandle* conn_handle =
      server()->FindConnectionForTesting(connected_id);

  ASSERT_TRUE(conn_handle);
  EXPECT_EQ(conn_handle->bondable_mode(),
            bondable ? bt::sm::BondableMode::Bondable : bt::sm::BondableMode::NonBondable);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest,
       RestartStartAdvertisingDuringInboundConnKeepsNewAdvAlive) {
  fble::Peer peer;
  // `conn` is stored so that the connection is not dropped immediately after connection.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  FidlAdvHandle first_token, second_token;

  fble::AdvertisingParameters params;
  params.set_connectable(true);
  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->StartAdvertising(std::move(params), first_token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_ok());

  fit::closure complete_interrogation;
  // Hang interrogation so we can control when the inbound connection procedure completes.
  test_device()->pause_responses_for_opcode(
      bt::hci::kReadRemoteVersionInfo,
      [&](fit::closure trigger) { complete_interrogation = std::move(trigger); });

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();

  EXPECT_FALSE(peer.has_id());
  EXPECT_FALSE(conn.is_valid());
  // test_device()->ConnectLowEnergy caused interrogation as part of the inbound GAP connection
  // process, so this closure should be filled in.
  ASSERT_TRUE(complete_interrogation);

  // Hang the SetAdvertisingParameters HCI command so we can invoke the advertising status callback
  // after connection completion.
  fit::closure complete_start_advertising;
  test_device()->pause_responses_for_opcode(
      bt::hci::kLESetAdvertisingParameters,
      [&](fit::closure trigger) { complete_start_advertising = std::move(trigger); });

  // Restart advertising during inbound connection, simulating the race seen in fxbug.dev/72825.
  result = std::nullopt;
  server()->StartAdvertising(fble::AdvertisingParameters{}, second_token.NewRequest(),
                             [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(complete_start_advertising);
  // Advertising shouldn't complete until we trigger the above closure
  EXPECT_FALSE(result.has_value());
  // The first AdvertisingHandle should be closed, as we have started a second advertisement.
  EXPECT_TRUE(bt::IsChannelPeerClosed(first_token.channel()));

  // Allow interrogation to complete, enabling the connection process to proceed.
  complete_interrogation();
  RunLoopUntilIdle();
  // Now peer should be connected
  EXPECT_TRUE(peer.has_id());
  EXPECT_TRUE(conn.is_valid());

  // Allow second StartAdvertising to complete.
  complete_start_advertising();
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_ok());
  // The second advertising handle should still be active.
  EXPECT_FALSE(bt::IsChannelPeerClosed(second_token.channel()));
}

// Ensures that a connection to a canceled advertisement received after the advertisement is
// canceled doesn't end or get sent to a new advertisement.
TEST_F(FIDL_LowEnergyPeripheralServerTest, RestartAdvertiseDuringInboundConnKeepsNewAdvAlive) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_0;
  FakeAdvertisedPeripheral adv_peripheral_server_0(adv_peripheral_handle_0.NewRequest());

  fble::AdvertisingParameters params;
  params.set_connectable(true);
  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle_0),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  fit::closure complete_interrogation;
  // Hang interrogation so we can control when the inbound connection procedure completes.
  test_device()->pause_responses_for_opcode(
      bt::hci::kReadRemoteVersionInfo,
      [&](fit::closure trigger) { complete_interrogation = std::move(trigger); });

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_peripheral_server_0.last_connected_peer());
  // test_device()->ConnectLowEnergy caused interrogation as part of the inbound GAP connection
  // process, so this closure should be filled in.
  ASSERT_TRUE(complete_interrogation);

  // Cancel the first advertisement.
  adv_peripheral_server_0.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_ok());

  // Hang the SetAdvertisingParameters HCI command so we can invoke the advertising status callback
  // of the second advertising request after connection completion.
  fit::closure complete_start_advertising;
  test_device()->pause_responses_for_opcode(
      bt::hci::kLESetAdvertisingParameters,
      [&](fit::closure trigger) { complete_start_advertising = std::move(trigger); });

  // Restart advertising during inbound connection, simulating the race seen in fxbug.dev/72825.
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_1;
  FakeAdvertisedPeripheral adv_peripheral_server_1(adv_peripheral_handle_1.NewRequest());
  bool server_1_closed = false;
  adv_peripheral_server_1.set_error_handler([&](auto) { server_1_closed = true; });
  result = std::nullopt;
  server()->Advertise(fble::AdvertisingParameters{}, std::move(adv_peripheral_handle_1),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(complete_start_advertising);
  EXPECT_FALSE(result.has_value());

  // Allow interrogation to complete, enabling the connection process to proceed.
  complete_interrogation();
  RunLoopUntilIdle();
  // The connection should have been dropped.
  EXPECT_FALSE(adv_peripheral_server_1.last_connected_peer());
  EXPECT_FALSE(adv_peripheral_server_0.last_connected_peer());

  // Allow second StartAdvertising to complete.
  complete_start_advertising();
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(server_1_closed);
  EXPECT_FALSE(adv_peripheral_server_1.last_connected_peer());

  adv_peripheral_server_1.Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(result.has_value());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest_FakeAdapter,
       StartAdvertisingWithIncludeTxPowerSetToTrue) {
  fble::AdvertisingParameters params;
  fble::AdvertisingData adv_data;
  adv_data.set_include_tx_power_level(true);
  params.set_data(std::move(adv_data));

  FidlAdvHandle token;

  server()->StartAdvertising(std::move(params), token.NewRequest(), [&](auto) {});
  RunLoopUntilIdle();
  ASSERT_EQ(adapter()->fake_le()->registered_advertisements().size(), 1u);
  EXPECT_TRUE(
      adapter()->fake_le()->registered_advertisements().begin()->second.include_tx_power_level);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest_FakeAdapter, AdvertiseWithIncludeTxPowerSetToTrue) {
  fble::AdvertisingParameters params;
  fble::AdvertisingData adv_data;
  adv_data.set_include_tx_power_level(true);
  params.set_data(std::move(adv_data));

  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  std::optional<fpromise::result<void, fble::PeripheralError>> result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_EQ(adapter()->fake_le()->registered_advertisements().size(), 1u);
  EXPECT_TRUE(
      adapter()->fake_le()->registered_advertisements().begin()->second.include_tx_power_level);

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
}

TEST_F(FIDL_LowEnergyPeripheralServerTest_FakeAdapter, AdvertiseInvalidAdvData) {
  fble::AdvertisingData adv_data;
  adv_data.set_name(std::string(bt::kMaxNameLength + 1, '*'));
  fble::AdvertisingParameters params;
  params.set_data(std::move(adv_data));

  fidl::InterfaceHandle<fble::AdvertisedPeripheral> advertised_peripheral_client;
  fidl::InterfaceRequest<fble::AdvertisedPeripheral> advertised_peripheral_server =
      advertised_peripheral_client.NewRequest();

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(advertised_peripheral_client),
                      [&](auto result) { adv_result = std::move(result); });
  RunLoopUntilIdle();
  EXPECT_EQ(adapter()->fake_le()->registered_advertisements().size(), 0u);
  ASSERT_TRUE(adv_result);
  EXPECT_TRUE(adv_result.value().is_error());
  EXPECT_EQ(adv_result->error(), fble::PeripheralError::INVALID_PARAMETERS);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest_FakeAdapter, AdvertiseInvalidScanResponseData) {
  fble::AdvertisingData adv_data;
  adv_data.set_name(std::string(bt::kMaxNameLength + 1, '*'));
  fble::AdvertisingParameters params;
  params.set_scan_response(std::move(adv_data));

  fidl::InterfaceHandle<fble::AdvertisedPeripheral> advertised_peripheral_client;
  fidl::InterfaceRequest<fble::AdvertisedPeripheral> advertised_peripheral_server =
      advertised_peripheral_client.NewRequest();

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(advertised_peripheral_client),
                      [&](auto result) { adv_result = std::move(result); });
  RunLoopUntilIdle();
  EXPECT_EQ(adapter()->fake_le()->registered_advertisements().size(), 0u);
  ASSERT_TRUE(adv_result);
  EXPECT_TRUE(adv_result.value().is_error());
  EXPECT_EQ(adv_result->error(), fble::PeripheralError::INVALID_PARAMETERS);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseAndReceiveTwoConnections) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  params.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { adv_result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result.has_value());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_peripheral_server.last_connected_peer());

  // Sending response to first connection should restart advertising.
  adv_peripheral_server.connections()[0].callback();
  RunLoopUntilIdle();

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr2));
  test_device()->ConnectLowEnergy(kTestAddr2);
  RunLoopUntilIdle();
  ASSERT_EQ(adv_peripheral_server.connections().size(), 2u);

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result.has_value());
  EXPECT_TRUE(adv_result->is_ok());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseCanceledBeforeAdvertisingStarts) {
  fit::closure send_adv_enable_response;
  test_device()->pause_responses_for_opcode(
      bt::hci::kLESetAdvertisingEnable,
      [&](fit::closure send_rsp) { send_adv_enable_response = std::move(send_rsp); });

  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { adv_result = std::move(cb_result); });
  RunLoopUntilIdle();
  ASSERT_TRUE(send_adv_enable_response);

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  send_adv_enable_response();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result.has_value());
  EXPECT_TRUE(adv_result->is_ok());
}

TEST_P(BoolParam, AdvertiseTwiceCausesSecondToFail) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_0;
  FakeAdvertisedPeripheral adv_peripheral_server_0(adv_peripheral_handle_0.NewRequest());
  bool adv_peripheral_server_0_closed = false;
  adv_peripheral_server_0.set_error_handler([&](auto) { adv_peripheral_server_0_closed = true; });

  fble::AdvertisingParameters params_0;
  fble::ConnectionOptions conn_opts;
  params_0.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result_0;
  server()->Advertise(std::move(params_0), std::move(adv_peripheral_handle_0),
                      [&](auto cb_result) { adv_result_0 = std::move(cb_result); });

  // Test both with and without running the loop between Advertise requests.
  if (GetParam()) {
    RunLoopUntilIdle();
    EXPECT_FALSE(adv_result_0.has_value());
    EXPECT_FALSE(adv_peripheral_server_0_closed);
  }

  fble::AdvertisedPeripheralHandle adv_peripheral_handle_1;
  FakeAdvertisedPeripheral adv_peripheral_server_1(adv_peripheral_handle_1.NewRequest());
  bool adv_peripheral_server_1_closed = false;
  adv_peripheral_server_1.set_error_handler([&](auto) { adv_peripheral_server_1_closed = true; });
  fble::AdvertisingParameters params_1;
  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result_1;
  server()->Advertise(std::move(params_1), std::move(adv_peripheral_handle_1),
                      [&](auto cb_result) { adv_result_1 = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result_0.has_value());
  EXPECT_FALSE(adv_peripheral_server_0_closed);
  ASSERT_TRUE(adv_result_1.has_value());
  ASSERT_TRUE(adv_result_1->is_error());
  EXPECT_EQ(adv_result_1->error(), fble::PeripheralError::FAILED);
  EXPECT_TRUE(adv_peripheral_server_1_closed);

  // Server 0 should still receive connections.
  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_peripheral_server_0.last_connected_peer());

  adv_peripheral_server_0.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result_0.has_value());
  EXPECT_TRUE(adv_result_0->is_ok());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, CallAdvertiseTwiceSequentiallyBothSucceed) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle_0;
  FakeAdvertisedPeripheral adv_peripheral_server_0(adv_peripheral_handle_0.NewRequest());
  fble::AdvertisingParameters params_0;
  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result_0;
  server()->Advertise(std::move(params_0), std::move(adv_peripheral_handle_0),
                      [&](auto cb_result) { adv_result_0 = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result_0.has_value());

  adv_peripheral_server_0.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result_0.has_value());
  EXPECT_TRUE(adv_result_0->is_ok());

  fble::AdvertisedPeripheralHandle adv_peripheral_handle_1;
  FakeAdvertisedPeripheral adv_peripheral_server_1(adv_peripheral_handle_1.NewRequest());

  fble::AdvertisingParameters params_1;
  fble::ConnectionOptions conn_opts;
  params_1.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result_1;
  server()->Advertise(std::move(params_1), std::move(adv_peripheral_handle_1),
                      [&](auto cb_result) { adv_result_1 = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result_1.has_value());

  // Server 1 should receive connections.
  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_peripheral_server_1.last_connected_peer());

  adv_peripheral_server_1.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result_1.has_value());
  EXPECT_TRUE(adv_result_1->is_ok());
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, PeerDisconnectClosesConnection) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  params.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { adv_result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result.has_value());

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_peripheral_server.last_connected_peer());
  fidl::InterfacePtr<fble::Connection> connection =
      adv_peripheral_server.connections()[0].connection.Bind();
  bool connection_closed = false;
  connection.set_error_handler([&](auto) { connection_closed = true; });
  EXPECT_FALSE(connection_closed);
  RunLoopUntilIdle();

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  ASSERT_TRUE(adv_result.has_value());
  EXPECT_TRUE(adv_result->is_ok());
  EXPECT_FALSE(connection_closed);

  test_device()->Disconnect(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_TRUE(connection_closed);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, IncomingConnectionFailureContinuesAdvertising) {
  fble::AdvertisedPeripheralHandle adv_peripheral_handle;
  FakeAdvertisedPeripheral adv_peripheral_server(adv_peripheral_handle.NewRequest());

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  params.set_connection_options(std::move(conn_opts));

  std::optional<fpromise::result<void, fble::PeripheralError>> adv_result;
  server()->Advertise(std::move(params), std::move(adv_peripheral_handle),
                      [&](auto cb_result) { adv_result = std::move(cb_result); });
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_result.has_value());

  // Cause peer interrogation to fail. This will result in a connection error status to be
  // received. Advertising should be immediately resumed, allowing future connections.
  test_device()->SetDefaultCommandStatus(bt::hci::kReadRemoteVersionInfo,
                                         bt::hci::StatusCode::kUnsupportedRemoteFeature);

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_peripheral_server.last_connected_peer());
  EXPECT_FALSE(adv_result.has_value());

  // Allow next interrogation to succeed.
  test_device()->ClearDefaultCommandStatus(bt::hci::kReadRemoteVersionInfo);

  test_device()->AddPeer(std::make_unique<FakePeer>(kTestAddr));
  test_device()->ConnectLowEnergy(kTestAddr);
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_peripheral_server.last_connected_peer());
  EXPECT_FALSE(adv_result.has_value());

  adv_peripheral_server.Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_result.has_value());
}

INSTANTIATE_TEST_SUITE_P(FIDL_LowEnergyPeripheralServerTest, BoolParam, ::testing::Bool());

}  // namespace
}  // namespace bthost
