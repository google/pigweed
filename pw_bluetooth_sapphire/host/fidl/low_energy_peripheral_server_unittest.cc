// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/fidl/low_energy_peripheral_server.h"

#include "adapter_test_fixture.h"
#include "fuchsia/bluetooth/cpp/fidl.h"
#include "fuchsia/bluetooth/le/cpp/fidl.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bthost {
namespace {

namespace fble = fuchsia::bluetooth::le;
const bt::DeviceAddress kTestAddr(bt::DeviceAddress::Type::kLEPublic, {0x01, 0, 0, 0, 0, 0});

using bt::testing::FakePeer;
using FidlAdvHandle = fidl::InterfaceHandle<fble::AdvertisingHandle>;

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

// Tests that aborting a StartAdvertising command sequence does not cause a crash in successive
// requests.
TEST_F(FIDL_LowEnergyPeripheralServerTest, StartAdvertisingWhilePendingDoesNotCrash) {
  fble::AdvertisingParameters params1, params2, params3;
  FidlAdvHandle token1, token2, token3;

  std::optional<fit::result<void, fble::PeripheralError>> result1, result2, result3;
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

  std::optional<fit::result<void, fble::PeripheralError>> result1, result2, result3, result4;
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

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseNoConnectionRelatedParamsNoConnection) {
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

  std::optional<fit::result<void, fble::PeripheralError>> result;
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

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseConnectableParameterTrueConnectsBondable) {
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

  std::optional<fit::result<void, fble::PeripheralError>> result;
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

  std::optional<fit::result<void, fble::PeripheralError>> result;
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

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseBondableConnectsBondable) {
  fble::Peer peer;
  // This is needed to hold onto the connection sent by the OnPeerConnected event. Otherwise the
  // resulting connection is dropped immediately after the call to test_device()->ConnectLowEnergy
  // completes and we cannot check whether it is bondable at the end of the test.
  fidl::InterfaceHandle<fble::Connection> conn;
  auto peer_connected_cb = [&](auto cb_peer, auto cb_conn) {
    peer = std::move(cb_peer);
    conn = std::move(cb_conn);
  };
  SetOnPeerConnectedCallback(peer_connected_cb);

  fble::AdvertisingParameters params;
  fble::ConnectionOptions conn_opts;
  conn_opts.set_bondable_mode(true);
  params.set_connection_options(std::move(conn_opts));

  FidlAdvHandle token;

  std::optional<fit::result<void, fble::PeripheralError>> result;
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

TEST_F(FIDL_LowEnergyPeripheralServerTest, AdvertiseNonBondableConnectsNonBondable) {
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
  conn_opts.set_bondable_mode(false);
  params.set_connection_options(std::move(conn_opts));

  FidlAdvHandle token;

  std::optional<fit::result<void, fble::PeripheralError>> result;
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
  ASSERT_EQ(conn_handle->bondable_mode(), bt::sm::BondableMode::NonBondable);
}

TEST_F(FIDL_LowEnergyPeripheralServerTest, RestartAdvDuringInboundConnKeepsNewAdvAlive) {
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
  std::optional<fit::result<void, fble::PeripheralError>> result;
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

}  // namespace
}  // namespace bthost
