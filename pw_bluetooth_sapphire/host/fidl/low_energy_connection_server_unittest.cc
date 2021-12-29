// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_server.h"

#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"

namespace bthost {
namespace {

namespace fble = fuchsia::bluetooth::le;
namespace fbg = fuchsia::bluetooth::gatt2;

const bt::DeviceAddress kTestAddr(bt::DeviceAddress::Type::kLEPublic, {0x01, 0, 0, 0, 0, 0});

class LowEnergyConnectionServerTest : public bthost::testing::AdapterTestFixture {
 public:
  LowEnergyConnectionServerTest() = default;
  ~LowEnergyConnectionServerTest() override = default;

  void SetUp() override {
    bthost::testing::AdapterTestFixture::SetUp();

    fidl::InterfaceHandle<fuchsia::bluetooth::le::Connection> handle;
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection = EstablishConnection();
    server_ = std::make_unique<LowEnergyConnectionServer>(
        gatt()->AsWeakPtr(), std::move(connection), handle.NewRequest().TakeChannel(),
        /*closed_cb=*/[this] {
          server_closed_cb_called_ = true;
          server_.reset();
        });
    client_ = handle.Bind();
  }

  fble::Connection* client() { return client_.get(); }

  void UnbindClient() { client_.Unbind(); }

  bt::PeerId peer_id() const { return peer_id_; }

  bool server_closed_cb_called() const { return server_closed_cb_called_; }

 private:
  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> EstablishConnection() {
    // Since LowEnergyConnectionHandle must be created by LowEnergyConnectionManager, we discover
    // and connect to a fake peer to get a LowEnergyConnectionHandle.
    std::unique_ptr<bt::testing::FakePeer> fake_peer =
        std::make_unique<bt::testing::FakePeer>(kTestAddr, /*connectable=*/true);
    test_device()->AddPeer(std::move(fake_peer));

    std::optional<bt::PeerId> peer_id;
    bt::gap::LowEnergyDiscoverySessionPtr session;
    adapter()->le()->StartDiscovery(
        /*active=*/true, [&](bt::gap::LowEnergyDiscoverySessionPtr cb_session) {
          session = std::move(cb_session);
          session->SetResultCallback(
              [&](const bt::gap::Peer& peer) { peer_id = peer.identifier(); });
        });
    RunLoopUntilIdle();
    ZX_ASSERT(peer_id);
    peer_id_ = *peer_id;

    std::optional<bt::gap::LowEnergyConnectionManager::ConnectionResult> conn_result;
    adapter()->le()->Connect(
        peer_id_,
        [&](bt::gap::LowEnergyConnectionManager::ConnectionResult result) {
          conn_result = std::move(result);
        },
        bt::gap::LowEnergyConnectionOptions());
    RunLoopUntilIdle();
    ZX_ASSERT(conn_result);
    ZX_ASSERT(conn_result->is_ok());
    return std::move(*conn_result).value();
  }

  std::unique_ptr<LowEnergyConnectionServer> server_;
  fble::ConnectionPtr client_;
  bool server_closed_cb_called_ = false;
  bt::PeerId peer_id_;
};

TEST_F(LowEnergyConnectionServerTest, RequestGattClientTwice) {
  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_0;
  client()->RequestGattClient(handle_0.NewRequest());
  fbg::ClientPtr client_0 = handle_0.Bind();
  std::optional<zx_status_t> client_epitaph_0;
  client_0.set_error_handler([&](zx_status_t epitaph) { client_epitaph_0 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_0);

  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_1;
  client()->RequestGattClient(handle_1.NewRequest());
  fbg::ClientPtr client_1 = handle_1.Bind();
  std::optional<zx_status_t> client_epitaph_1;
  client_1.set_error_handler([&](zx_status_t epitaph) { client_epitaph_1 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_0);
  ASSERT_TRUE(client_epitaph_1);
  EXPECT_EQ(client_epitaph_1.value(), ZX_ERR_ALREADY_BOUND);
}

TEST_F(LowEnergyConnectionServerTest, GattClientServerError) {
  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_0;
  client()->RequestGattClient(handle_0.NewRequest());
  fbg::ClientPtr client_0 = handle_0.Bind();
  std::optional<zx_status_t> client_epitaph_0;
  client_0.set_error_handler([&](zx_status_t epitaph) { client_epitaph_0 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_0);

  // Calling WatchServices twice should cause a server error.
  client_0->WatchServices({}, [](auto, auto) {});
  client_0->WatchServices({}, [](auto, auto) {});
  RunLoopUntilIdle();
  EXPECT_TRUE(client_epitaph_0);

  // Requesting a new GATT client should succeed.
  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_1;
  client()->RequestGattClient(handle_1.NewRequest());
  fbg::ClientPtr client_1 = handle_1.Bind();
  std::optional<zx_status_t> client_epitaph_1;
  client_1.set_error_handler([&](zx_status_t epitaph) { client_epitaph_1 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_1);
}

TEST_F(LowEnergyConnectionServerTest, RequestGattClientThenUnbindThenRequestAgainShouldSucceed) {
  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_0;
  client()->RequestGattClient(handle_0.NewRequest());
  fbg::ClientPtr client_0 = handle_0.Bind();
  std::optional<zx_status_t> client_epitaph_0;
  client_0.set_error_handler([&](zx_status_t epitaph) { client_epitaph_0 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_0);
  client_0.Unbind();
  RunLoopUntilIdle();

  // Requesting a new GATT client should succeed.
  fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::Client> handle_1;
  client()->RequestGattClient(handle_1.NewRequest());
  fbg::ClientPtr client_1 = handle_1.Bind();
  std::optional<zx_status_t> client_epitaph_1;
  client_1.set_error_handler([&](zx_status_t epitaph) { client_epitaph_1 = epitaph; });
  RunLoopUntilIdle();
  EXPECT_FALSE(client_epitaph_1);
}

TEST_F(LowEnergyConnectionServerTest, ServerClosedOnConnectionClosed) {
  adapter()->le()->Disconnect(peer_id());
  RunLoopUntilIdle();
  EXPECT_TRUE(server_closed_cb_called());
}

TEST_F(LowEnergyConnectionServerTest, ServerClosedWhenFIDLClientClosesConnection) {
  UnbindClient();
  RunLoopUntilIdle();
  EXPECT_TRUE(server_closed_cb_called());
}

}  // namespace
}  // namespace bthost
