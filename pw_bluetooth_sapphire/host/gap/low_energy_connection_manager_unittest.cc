// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"

#include <zircon/assert.h>

#include <memory>
#include <vector>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {
namespace gap {
namespace {

using bt::testing::FakeController;
using bt::testing::FakePeer;

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

const DeviceAddress kAddress0(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kAddrAlias0(DeviceAddress::Type::kBREDR, kAddress0.value());
const DeviceAddress kAddress1(DeviceAddress::Type::kLERandom, {2});
const DeviceAddress kAddress2(DeviceAddress::Type::kBREDR, {3});

const size_t kLEMaxNumPackets = 10;
const hci::DataBufferInfo kLEDataBufferInfo(hci::kMaxACLPayloadSize, kLEMaxNumPackets);

class LowEnergyConnectionManagerTest : public TestingBase {
 public:
  LowEnergyConnectionManagerTest() = default;
  ~LowEnergyConnectionManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    // Initialize with LE buffers only.
    TestingBase::InitializeACLDataChannel(hci::DataBufferInfo(), kLEDataBufferInfo);

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    peer_cache_ = std::make_unique<PeerCache>();
    l2cap_ = data::testing::FakeDomain::Create();
    l2cap_->Initialize();

    connector_ = std::make_unique<hci::LowEnergyConnector>(
        transport(), &addr_delegate_, dispatcher(),
        fit::bind_member(this, &LowEnergyConnectionManagerTest::OnIncomingConnection));

    conn_mgr_ = std::make_unique<LowEnergyConnectionManager>(
        transport(), &addr_delegate_, connector_.get(), peer_cache_.get(), l2cap_,
        gatt::testing::FakeLayer::Create());

    test_device()->set_connection_state_callback(
        fit::bind_member(this, &LowEnergyConnectionManagerTest::OnConnectionStateChanged));
    StartTestDevice();
  }

  void TearDown() override {
    if (conn_mgr_)
      conn_mgr_ = nullptr;
    peer_cache_ = nullptr;

    l2cap_->ShutDown();
    l2cap_ = nullptr;

    TestingBase::TearDown();
  }

  // Deletes |conn_mgr_|.
  void DeleteConnMgr() { conn_mgr_ = nullptr; }

  PeerCache* peer_cache() const { return peer_cache_.get(); }
  LowEnergyConnectionManager* conn_mgr() const { return conn_mgr_.get(); }
  data::testing::FakeDomain* fake_l2cap() const { return l2cap_.get(); }

  // Addresses of currently connected fake peers.
  using PeerList = std::unordered_set<DeviceAddress>;
  const PeerList& connected_peers() const { return connected_peers_; }

  // Addresses of peers with a canceled connection attempt.
  const PeerList& canceled_peers() const { return canceled_peers_; }

  hci::ConnectionPtr MoveLastRemoteInitiated() { return std::move(last_remote_initiated_); }

 private:
  // Called by |connector_| when a new remote initiated connection is received.
  void OnIncomingConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                            const DeviceAddress& peer_address,
                            const hci::LEConnectionParameters& conn_params) {
    DeviceAddress local_address(DeviceAddress::Type::kLEPublic, {3, 2, 1, 1, 2, 3});

    // Create a production connection object that can interact with the fake
    // controller.
    last_remote_initiated_ = hci::Connection::CreateLE(handle, role, local_address, peer_address,
                                                       conn_params, transport());
  }

  // Called by FakeController on connection events.
  void OnConnectionStateChanged(const DeviceAddress& address, hci::ConnectionHandle handle,
                                bool connected, bool canceled) {
    bt_log(TRACE, "gap-test",
           "OnConnectionStateChanged: : %s (handle: %#.4x) (connected: %s) (canceled: %s):\n",
           address.ToString().c_str(), handle, (connected ? "true" : "false"),
           (canceled ? "true" : "false"));
    if (canceled) {
      canceled_peers_.insert(address);
    } else if (connected) {
      ZX_DEBUG_ASSERT(connected_peers_.find(address) == connected_peers_.end());
      connected_peers_.insert(address);
    } else {
      ZX_DEBUG_ASSERT(connected_peers_.find(address) != connected_peers_.end());
      connected_peers_.erase(address);
    }
  }

  fbl::RefPtr<data::testing::FakeDomain> l2cap_;

  hci::FakeLocalAddressDelegate addr_delegate_;
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<hci::LowEnergyConnector> connector_;
  std::unique_ptr<LowEnergyConnectionManager> conn_mgr_;

  // The most recent remote-initiated connection reported by |connector_|.
  hci::ConnectionPtr last_remote_initiated_;

  PeerList connected_peers_;
  PeerList canceled_peers_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionManagerTest);
};

using GAP_LowEnergyConnectionManagerTest = LowEnergyConnectionManagerTest;

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectUnknownPeer) {
  constexpr PeerId kUnknownId(1);
  EXPECT_FALSE(conn_mgr()->Connect(kUnknownId, {}));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectClassicPeer) {
  auto* peer = peer_cache()->NewPeer(kAddress2, true);
  EXPECT_FALSE(conn_mgr()->Connect(peer->identifier(), {}));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectNonConnectablePeer) {
  auto* peer = peer_cache()->NewPeer(kAddress0, false);
  EXPECT_FALSE(conn_mgr()->Connect(peer->identifier(), {}));
}

// An error is received via the HCI Command cb_status event
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSinglePeerErrorStatus) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_status(hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished, status.protocol_error());
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());
}

// LE Connection Complete event reports error
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSinglePeerFailure) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_response(hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished, status.protocol_error());
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSinglePeerTimeout) {
  constexpr zx::duration kTestRequestTimeout = zx::sec(20);

  auto* peer = peer_cache()->NewPeer(kAddress0, true);

  // We add no fake peers to cause the request to time out.

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  conn_mgr()->set_request_timeout_for_testing(kTestRequestTimeout);
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopFor(kTestRequestTimeout);

  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());
}

// Tests that an entry in the cache does not expire while a connection attempt
// is pending.
TEST_F(GAP_LowEnergyConnectionManagerTest, PeerDoesNotExpireDuringTimeout) {
  // Set a connection timeout that is longer than the PeerCache expiry
  // timeout.
  // TODO(BT-825): Consider configuring the cache timeout explicitly rather than
  // relying on the kCacheTimeout constant.
  constexpr zx::duration kTestRequestTimeout = kCacheTimeout + zx::sec(1);
  conn_mgr()->set_request_timeout_for_testing(kTestRequestTimeout);

  // Note: Use a random address so that the peer becomes temporary upon failure.
  auto* peer = peer_cache()->NewPeer(kAddress1, true);
  EXPECT_TRUE(peer->temporary());

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());
  EXPECT_FALSE(peer->temporary());

  RunLoopFor(kTestRequestTimeout);
  EXPECT_EQ(HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_EQ(peer, peer_cache()->FindByAddress(kAddress1));
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());
  EXPECT_TRUE(peer->temporary());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, PeerDoesNotExpireDuringDelayedConnect) {
  // Make the connection resolve after a delay that is longer than the cache
  // timeout.
  constexpr zx::duration kConnectionDelay = kCacheTimeout + zx::sec(1);
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  settings.le_connection_delay = kConnectionDelay;
  test_device()->set_settings(settings);

  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto id = peer->identifier();
  EXPECT_TRUE(peer->temporary());

  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Make sure the connection request doesn't time out while waiting for a
  // response.
  conn_mgr()->set_request_timeout_for_testing(kConnectionDelay + zx::sec(1));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);

    ASSERT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };
  EXPECT_TRUE(conn_mgr()->Connect(id, callback));
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopFor(kConnectionDelay);
  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(status);

  // The peer should not have expired during this time.
  peer = peer_cache()->FindByAddress(kAddress0);
  ASSERT_TRUE(peer);
  EXPECT_EQ(id, peer->identifier());
  EXPECT_TRUE(peer->connected());
  EXPECT_FALSE(peer->temporary());
}

// Successful connection to single peer
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSinglePeer) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  EXPECT_TRUE(peer->temporary());

  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };

  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));

  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(conn_ref->active());
  EXPECT_EQ(peer->identifier(), conn_ref->peer_identifier());
  EXPECT_FALSE(peer->temporary());
  EXPECT_EQ(Peer::ConnectionState::kConnected, peer->le()->connection_state());
}

struct TestObject final : fbl::RefCounted<TestObject> {
  explicit TestObject(bool* d) : deleted(d) {
    ZX_DEBUG_ASSERT(deleted);
    *deleted = false;
  }

  ~TestObject() { *deleted = true; }

  bool* deleted;
};

TEST_F(GAP_LowEnergyConnectionManagerTest, DeleteRefInClosedCallback) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  bool deleted = false;
  auto obj = fbl::AdoptRef(new TestObject(&deleted));
  LowEnergyConnectionRefPtr conn_ref;
  int closed_count = 0;
  auto closed_cb = [&, obj = std::move(obj)] {
    closed_count++;
    conn_ref = nullptr;

    // The object should remain alive for the duration of this callback.
    EXPECT_FALSE(deleted);
  };

  auto success_cb = [&conn_ref, &closed_cb](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(cb_conn_ref);
    conn_ref = std::move(cb_conn_ref);
    conn_ref->set_closed_callback(std::move(closed_cb));
  };

  ASSERT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(conn_ref->active());

  // This will trigger the closed callback.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));
  RunLoopUntilIdle();

  EXPECT_EQ(1, closed_count);
  EXPECT_TRUE(connected_peers().empty());
  EXPECT_FALSE(conn_ref);

  // The object should be deleted.
  EXPECT_TRUE(deleted);
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ReleaseRef) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };

  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, connected_peers().size());
  ASSERT_TRUE(peer->le());
  EXPECT_EQ(Peer::ConnectionState::kConnected, peer->le()->connection_state());

  ASSERT_TRUE(conn_ref);
  conn_ref = nullptr;

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_peers().empty());
  EXPECT_EQ(Peer::ConnectionState::kNotConnected, peer->le()->connection_state());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, OnePeerTwoPendingRequestsBothFail) {
  constexpr int kRequestCount = 2;

  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_response(hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status statuses[kRequestCount];

  int cb_count = 0;
  auto callback = [&statuses, &cb_count](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    statuses[cb_count++] = cb_status;
  };

  for (int i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback)) << "request count: " << i + 1;
  }

  RunLoopUntilIdle();

  ASSERT_EQ(kRequestCount, cb_count);
  for (int i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(statuses[i].is_protocol_error());
    EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished, statuses[i].protocol_error())
        << "request count: " << i + 1;
  }
}

TEST_F(GAP_LowEnergyConnectionManagerTest, OnePeerManyPendingRequests) {
  constexpr size_t kRequestCount = 50;

  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  for (size_t i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback)) << "request count: " << i + 1;
  }

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));

  EXPECT_EQ(kRequestCount, conn_refs.size());
  for (size_t i = 0; i < kRequestCount; ++i) {
    ASSERT_TRUE(conn_refs[i]);
    EXPECT_TRUE(conn_refs[i]->active());
    EXPECT_EQ(peer->identifier(), conn_refs[i]->peer_identifier());
  }

  // Release one reference. The rest should be active.
  conn_refs[0] = nullptr;
  for (size_t i = 1; i < kRequestCount; ++i)
    EXPECT_TRUE(conn_refs[i]->active());

  // Release all but one reference.
  for (size_t i = 1; i < kRequestCount - 1; ++i)
    conn_refs[i] = nullptr;
  EXPECT_TRUE(conn_refs[kRequestCount - 1]->active());

  // Drop the last reference.
  conn_refs[kRequestCount - 1] = nullptr;

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, AddRefAfterConnection) {
  constexpr size_t kRefCount = 50;

  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));
  EXPECT_EQ(1u, conn_refs.size());

  // Add new references.
  for (size_t i = 1; i < kRefCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback)) << "request count: " << i + 1;
    RunLoopUntilIdle();
  }

  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));
  EXPECT_EQ(kRefCount, conn_refs.size());

  // Disconnect.
  conn_refs.clear();

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, PendingRequestsOnTwoPeers) {
  auto* peer0 = peer_cache()->NewPeer(kAddress0, true);
  auto* peer1 = peer_cache()->NewPeer(kAddress1, true);

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress1));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer0->identifier(), callback));
  EXPECT_TRUE(conn_mgr()->Connect(peer1->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(2u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));
  EXPECT_EQ(1u, connected_peers().count(kAddress1));

  ASSERT_EQ(2u, conn_refs.size());
  ASSERT_TRUE(conn_refs[0]);
  ASSERT_TRUE(conn_refs[1]);
  EXPECT_EQ(peer0->identifier(), conn_refs[0]->peer_identifier());
  EXPECT_EQ(peer1->identifier(), conn_refs[1]->peer_identifier());

  // |peer1| should disconnect first.
  conn_refs[1] = nullptr;

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));

  conn_refs.clear();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, PendingRequestsOnTwoPeersOneFails) {
  auto* peer0 = peer_cache()->NewPeer(kAddress0, true);
  auto* peer1 = peer_cache()->NewPeer(kAddress1, true);

  auto fake_peer0 = std::make_unique<FakePeer>(kAddress0);
  fake_peer0->set_connect_response(hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer0));
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress1));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto, auto conn_ref) {
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer0->identifier(), callback));
  EXPECT_TRUE(conn_mgr()->Connect(peer1->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress1));

  ASSERT_EQ(2u, conn_refs.size());
  EXPECT_FALSE(conn_refs[0]);
  ASSERT_TRUE(conn_refs[1]);
  EXPECT_EQ(peer1->identifier(), conn_refs[1]->peer_identifier());

  // Both connections should disconnect.
  conn_refs.clear();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, Destructor) {
  auto* peer0 = peer_cache()->NewPeer(kAddress0, true);
  auto* peer1 = peer_cache()->NewPeer(kAddress1, true);

  // Connecting to this peer will succeed.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // Connecting to this peer will remain pending.
  auto pending_peer = std::make_unique<FakePeer>(kAddress1);
  pending_peer->set_force_pending_connect(true);
  test_device()->AddPeer(std::move(pending_peer));

  // Below we create one connection and one pending request to have at the time
  // of destruction.

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    EXPECT_TRUE(status);

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer0->identifier(), success_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(conn_ref);
  bool conn_closed = false;
  conn_ref->set_closed_callback([&conn_closed] { conn_closed = true; });

  bool error_cb_called = false;
  auto error_cb = [&error_cb_called](auto status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    EXPECT_EQ(HostError::kFailed, status.error());
    error_cb_called = true;
  };

  // This will send an HCI command to the fake controller. We delete the
  // connection manager before a connection event gets received which should
  // cancel the connection.
  EXPECT_TRUE(conn_mgr()->Connect(peer1->identifier(), error_cb));
  DeleteConnMgr();

  RunLoopUntilIdle();

  EXPECT_TRUE(error_cb_called);
  EXPECT_TRUE(conn_closed);
  EXPECT_EQ(1u, canceled_peers().size());
  EXPECT_EQ(1u, canceled_peers().count(kAddress1));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectPendingConnections) {
  auto* dev0 = peer_cache()->NewPeer(kAddress0, true);
  auto* dev1 = peer_cache()->NewPeer(kAddress1, true);

  auto callback = [](auto, auto) {};  // no-op

  EXPECT_TRUE(conn_mgr()->Connect(dev0->identifier(), callback));
  EXPECT_TRUE(conn_mgr()->Connect(dev1->identifier(), callback));
  EXPECT_EQ(Peer::ConnectionState::kInitializing, dev0->le()->connection_state());

  EXPECT_FALSE(conn_mgr()->Disconnect(dev0->identifier()));
  EXPECT_FALSE(conn_mgr()->Disconnect(dev1->identifier()));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectUnknownPeer) {
  // Unknown peers are inherently "not connected."
  EXPECT_TRUE(conn_mgr()->Disconnect(PeerId(999)));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectUnconnectedPeer) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // This returns true so long the peer is not connected.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, Disconnect) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  int closed_count = 0;
  auto closed_cb = [&closed_count] { closed_count++; };

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto success_cb = [&conn_refs, &closed_cb](auto status, auto conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    conn_ref->set_closed_callback(closed_cb);
    conn_refs.push_back(std::move(conn_ref));
  };

  // Issue two connection refs.
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));

  RunLoopUntilIdle();

  ASSERT_EQ(2u, conn_refs.size());

  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));

  RunLoopUntilIdle();

  EXPECT_EQ(2, closed_count);
  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(canceled_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectThrice) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  int closed_count = 0;
  auto closed_cb = [&closed_count] { closed_count++; };

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&closed_cb, &conn_ref](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    conn_ref = std::move(cb_conn_ref);
    ASSERT_TRUE(conn_ref);
    conn_ref->set_closed_callback(closed_cb);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));

  RunLoopUntilIdle();

  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));

  // Try to disconnect again while the first disconnection is in progress.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));

  RunLoopUntilIdle();

  // The single ref should get only one "closed" call.
  EXPECT_EQ(1, closed_count);
  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(canceled_peers().empty());

  // Try to disconnect once more, now that the link is gone.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));
}

// Tests when a link is lost without explicitly disconnecting
TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectEvent) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  int closed_count = 0;
  auto closed_cb = [&closed_count] { closed_count++; };

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto success_cb = [&conn_refs, &closed_cb](auto status, auto conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    conn_ref->set_closed_callback(closed_cb);
    conn_refs.push_back(std::move(conn_ref));
  };

  // Issue two connection refs.
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));

  RunLoopUntilIdle();

  ASSERT_EQ(2u, conn_refs.size());

  // This makes FakeController send us HCI Disconnection Complete events.
  test_device()->Disconnect(kAddress0);

  RunLoopUntilIdle();

  EXPECT_EQ(2, closed_count);
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectAfterRefsReleased) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));

  RunLoopUntilIdle();

  ASSERT_TRUE(conn_ref);
  conn_ref.reset();

  // Try to disconnect while the zero-refs connection is being disconnected.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(canceled_peers().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectWhileRefPending) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(cb_conn_ref);
    EXPECT_TRUE(cb_conn_ref->active());

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  auto ref_cb = [](auto status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    EXPECT_FALSE(status);
    EXPECT_EQ(HostError::kFailed, status.error());
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), ref_cb));

  // This should invalidate the ref that was bound to |ref_cb|.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer->identifier()));

  RunLoopUntilIdle();
}

// This tests that a connection reference callback returns nullptr if a HCI
// Disconnection Complete event is received for the corresponding ACL link
// BEFORE the callback gets run.
TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectCompleteEventWhileRefPending) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref](auto status, auto cb_conn_ref) {
    ASSERT_TRUE(cb_conn_ref);
    ASSERT_TRUE(status);
    EXPECT_TRUE(cb_conn_ref->active());

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  // Request a new reference. Disconnect the link before the reference is
  // received.
  size_t ref_cb_count = 0;
  auto ref_cb = [&ref_cb_count](auto status, auto conn_ref) {
    ref_cb_count++;
    EXPECT_FALSE(conn_ref);
    EXPECT_FALSE(status);
    EXPECT_EQ(HostError::kFailed, status.error());
  };

  size_t disconn_cb_count = 0;
  auto disconn_cb = [this, ref_cb, peer, &disconn_cb_count](auto) {
    disconn_cb_count++;
    // The link is gone but conn_mgr() hasn't updated the connection state yet.
    // The request to connect will attempt to add a new reference which will be
    // invalidated before |ref_cb| gets called.
    EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), ref_cb));
  };
  conn_mgr()->SetDisconnectCallbackForTesting(disconn_cb);

  test_device()->SendDisconnectionCompleteEvent(conn_ref->handle());

  RunLoopUntilIdle();

  EXPECT_EQ(1u, ref_cb_count);
  EXPECT_EQ(1u, disconn_cb_count);
}

TEST_F(GAP_LowEnergyConnectionManagerTest, RemovePeerFromPeerCacheDuringDisconnection) {
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(cb_conn_ref);
    EXPECT_TRUE(cb_conn_ref->active());

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), success_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  // This should invalidate the ref that was bound to |ref_cb|.
  const PeerId id = peer->identifier();
  EXPECT_TRUE(conn_mgr()->Disconnect(id));
  ASSERT_FALSE(peer->le()->connected());
  EXPECT_FALSE(conn_ref->active());

  EXPECT_TRUE(peer_cache()->RemoveDisconnectedPeer(id));

  RunLoopUntilIdle();

  EXPECT_FALSE(peer_cache()->FindById(id));
  EXPECT_FALSE(peer_cache()->FindByAddress(kAddress0));
}

// Listener receives remote initiated connection ref.
TEST_F(GAP_LowEnergyConnectionManagerTest, RegisterRemoteInitiatedLink) {
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // First create a fake incoming connection.
  test_device()->ConnectLowEnergy(kAddress0);

  RunLoopUntilIdle();

  auto link = MoveLastRemoteInitiated();
  ASSERT_TRUE(link);

  LowEnergyConnectionRefPtr conn_ref = conn_mgr()->RegisterRemoteInitiatedLink(std::move(link));
  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(conn_ref->active());

  // A Peer should now exist in the cache.
  auto* peer = peer_cache()->FindByAddress(kAddress0);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), conn_ref->peer_identifier());
  EXPECT_TRUE(peer->connected());
  EXPECT_TRUE(peer->le()->connected());

  conn_ref = nullptr;

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_peers().empty());
}

// Listener receives remote initiated connection ref for a known peer with the
// same BR/EDR address.
TEST_F(GAP_LowEnergyConnectionManagerTest, IncomingConnectionUpgradesKnownBrEdrPeerToDualMode) {
  Peer* peer = peer_cache()->NewPeer(kAddrAlias0, true);
  ASSERT_TRUE(peer);
  ASSERT_EQ(peer, peer_cache()->FindByAddress(kAddress0));
  ASSERT_EQ(TechnologyType::kClassic, peer->technology());

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // First create a fake incoming connection.
  test_device()->ConnectLowEnergy(kAddress0);

  RunLoopUntilIdle();

  auto link = MoveLastRemoteInitiated();
  ASSERT_TRUE(link);

  LowEnergyConnectionRefPtr conn_ref = conn_mgr()->RegisterRemoteInitiatedLink(std::move(link));
  ASSERT_TRUE(conn_ref);

  EXPECT_EQ(peer->identifier(), conn_ref->peer_identifier());
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());
}

// Successful connection to a peer whose address type is kBREDR.
// TODO(2761): This test will likely become obsolete when LE connections are based on the presence
// of LowEnergyData in a Peer and no address type enum exists.
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectAndDisconnectDualModeDeviceWithBrEdrAddress) {
  Peer* peer = peer_cache()->NewPeer(kAddrAlias0, true);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr());

  peer->MutLe();
  ASSERT_EQ(TechnologyType::kDualMode, peer->technology());
  ASSERT_EQ(peer, peer_cache()->FindByAddress(kAddress0));
  ASSERT_EQ(DeviceAddress::Type::kBREDR, peer->address().type());

  // Only the LE transport connects in this test, so only add an LE FakePeer to FakeController.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    ASSERT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };

  EXPECT_TRUE(connected_peers().empty());
  EXPECT_TRUE(conn_mgr()->Connect(peer->identifier(), callback));
  EXPECT_EQ(Peer::ConnectionState::kInitializing, peer->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, connected_peers().size());
  EXPECT_EQ(1u, connected_peers().count(kAddress0));

  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(conn_ref->active());
  EXPECT_EQ(peer->identifier(), conn_ref->peer_identifier());
  EXPECT_FALSE(peer->temporary());
  EXPECT_EQ(Peer::ConnectionState::kConnected, peer->le()->connection_state());

  conn_ref = nullptr;
  RunLoopUntilIdle();
  EXPECT_EQ(0u, connected_peers().size());
}

// Tests that the master accepts the connection parameters that are sent from
// a fake slave and eventually applies them to the link.
TEST_F(GAP_LowEnergyConnectionManagerTest, L2CAPLEConnectionParameterUpdate) {
  // Set up a fake peer and a connection over which to process the L2CAP
  // request.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  ASSERT_TRUE(peer);

  LowEnergyConnectionRefPtr conn_ref;
  auto conn_cb = [&conn_ref](const auto& peer_id, auto cr) { conn_ref = std::move(cr); };
  ASSERT_TRUE(conn_mgr()->Connect(peer->identifier(), conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  hci::LEPreferredConnectionParameters preferred(
      hci::kLEConnectionIntervalMin, hci::kLEConnectionIntervalMax, hci::kLEConnectionLatencyMax,
      hci::kLEConnectionSupervisionTimeoutMax);

  hci::LEConnectionParameters actual;
  bool fake_peer_cb_called = false;
  bool conn_params_cb_called = false;

  auto fake_peer_cb = [&actual, &fake_peer_cb_called](const auto& addr, const auto& params) {
    fake_peer_cb_called = true;
    actual = params;
  };
  test_device()->set_le_connection_parameters_callback(fake_peer_cb);

  auto conn_params_cb = [&conn_params_cb_called, &conn_ref](const auto& peer) {
    EXPECT_EQ(conn_ref->peer_identifier(), peer.identifier());
    conn_params_cb_called = true;
  };
  conn_mgr()->SetConnectionParametersCallbackForTesting(conn_params_cb);

  fake_l2cap()->TriggerLEConnectionParameterUpdate(conn_ref->handle(), preferred);

  RunLoopUntilIdle();

  EXPECT_TRUE(fake_peer_cb_called);
  ASSERT_TRUE(conn_params_cb_called);

  ASSERT_TRUE(peer->le());
  EXPECT_EQ(preferred, *peer->le()->preferred_connection_parameters());
  EXPECT_EQ(actual, *peer->le()->connection_parameters());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, L2CAPSignalLinkError) {
  // Set up a fake peer and a connection over which to process the L2CAP
  // request.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  auto* peer = peer_cache()->NewPeer(kAddress0, true);
  ASSERT_TRUE(peer);

  fbl::RefPtr<l2cap::Channel> att_chan;
  auto l2cap_chan_cb = [&att_chan](auto chan) { att_chan = chan; };
  fake_l2cap()->set_channel_callback(l2cap_chan_cb);

  LowEnergyConnectionRefPtr conn_ref;
  auto conn_cb = [&conn_ref](const auto& peer_id, auto cr) { conn_ref = std::move(cr); };
  ASSERT_TRUE(conn_mgr()->Connect(peer->identifier(), conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(att_chan);
  ASSERT_EQ(1u, connected_peers().size());

  // Signaling a link error through the channel should disconnect the link.
  att_chan->SignalLinkError();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_peers().empty());
}

// Test fixture for tests that disconnect a connection in various ways and expect that
// controller packet counts are not cleared on disconnecting, but are cleared on disconnection
// complete. Tests should disconnect conn_ref0().
class PendingPacketsTest : public LowEnergyConnectionManagerTest {
 public:
  PendingPacketsTest() = default;
  ~PendingPacketsTest() override = default;

  void SetUp() override {
    LowEnergyConnectionManagerTest::SetUp();
    const DeviceAddress kPeerAddr0(DeviceAddress::Type::kLEPublic, {1});
    const DeviceAddress kPeerAddr1(DeviceAddress::Type::kLEPublic, {2});

    peer0_ = peer_cache()->NewPeer(kPeerAddr0, true);
    EXPECT_TRUE(peer0_->temporary());
    test_device()->AddPeer(std::make_unique<FakePeer>(kPeerAddr0));

    peer1_ = peer_cache()->NewPeer(kPeerAddr1, true);
    EXPECT_TRUE(peer1_->temporary());
    test_device()->AddPeer(std::make_unique<FakePeer>(kPeerAddr1));

    // Connect |peer0|
    hci::Status status0(HostError::kFailed);
    conn_ref0_.reset();
    auto callback0 = [this, &status0](auto cb_status, auto cb_conn_ref) {
      EXPECT_TRUE(cb_conn_ref);
      status0 = cb_status;
      conn_ref0_ = std::move(cb_conn_ref);
      EXPECT_TRUE(conn_ref0_->active());
    };
    EXPECT_TRUE(conn_mgr()->Connect(peer0_->identifier(), callback0));
    RunLoopUntilIdle();

    // Connect |peer1|
    hci::Status status1(HostError::kFailed);
    conn_ref1_.reset();
    auto callback1 = [this, &status1](auto cb_status, auto cb_conn_ref) {
      EXPECT_TRUE(cb_conn_ref);
      status1 = cb_status;
      conn_ref1_ = std::move(cb_conn_ref);
      EXPECT_TRUE(conn_ref1_->active());
    };
    EXPECT_TRUE(conn_mgr()->Connect(peer1_->identifier(), callback1));
    RunLoopUntilIdle();

    packet_count_ = 0;
    test_device()->SetDataCallback([&](const auto&) { packet_count_++; }, dispatcher());
    test_device()->set_auto_completed_packets_event_enabled(false);
    test_device()->set_auto_disconnection_complete_event_enabled(false);

    // Fill controller buffer by sending |kMaxNumPackets| packets to peer0.
    for (size_t i = 0; i < kLEMaxNumPackets; i++) {
      ASSERT_TRUE(acl_data_channel()->SendPacket(
          hci::ACLDataPacket::New(conn_ref0_->handle(),
                                  hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                  hci::ACLBroadcastFlag::kPointToPoint, 1),
          l2cap::kInvalidChannelId));
    }

    // Queue packet for |peer1|.
    ASSERT_TRUE(acl_data_channel()->SendPacket(
        hci::ACLDataPacket::New(conn_ref1_->handle(),
                                hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                hci::ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId));

    RunLoopUntilIdle();

    // Packet for |peer1| should not have been sent because controller buffer is full.
    EXPECT_EQ(kLEMaxNumPackets, packet_count_);

    handle0_ = conn_ref0_->handle();
  }

  void TearDown() override {
    RunLoopUntilIdle();

    // Packet for |peer1| should not have been sent before Disconnection Complete event.
    EXPECT_EQ(kLEMaxNumPackets, packet_count_);

    // This makes FakeController send us the HCI Disconnection Complete event.
    test_device()->SendDisconnectionCompleteEvent(handle0_);
    RunLoopUntilIdle();

    // |peer0|'s link should have been unregistered.
    ASSERT_FALSE(acl_data_channel()->SendPacket(
        hci::ACLDataPacket::New(handle0_, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                hci::ACLBroadcastFlag::kPointToPoint, 1),
        l2cap::kInvalidChannelId));

    // Packet for |peer1| should have been sent.
    EXPECT_EQ(kLEMaxNumPackets + 1, packet_count_);

    peer0_ = nullptr;
    peer1_ = nullptr;
    conn_ref0_.reset();
    conn_ref1_.reset();

    LowEnergyConnectionManagerTest::TearDown();
  }

  Peer* peer0() { return peer0_; }
  LowEnergyConnectionRefPtr& conn_ref0() { return conn_ref0_; }

 private:
  size_t packet_count_;
  Peer* peer0_;
  Peer* peer1_;
  hci::ConnectionHandle handle0_;
  LowEnergyConnectionRefPtr conn_ref0_;
  LowEnergyConnectionRefPtr conn_ref1_;
};

using GAP_LowEnergyConnectionManagerPendingPacketsTest = PendingPacketsTest;

TEST_F(GAP_LowEnergyConnectionManagerPendingPacketsTest, Disconnect) {
  // Send HCI Disconnect to controller.
  EXPECT_TRUE(conn_mgr()->Disconnect(peer0()->identifier()));
}

TEST_F(GAP_LowEnergyConnectionManagerPendingPacketsTest, ReleaseRef) {
  // Releasing ref should send HCI Disconnect to controller.
  conn_ref0().reset();
}

}  // namespace
}  // namespace gap
}  // namespace bt
