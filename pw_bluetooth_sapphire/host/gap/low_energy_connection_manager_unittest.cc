// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"

#include <fbl/macros.h>
#include <zircon/assert.h>

#include <memory>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {
namespace gap {
namespace {

using bt::testing::FakeController;
using bt::testing::FakePeer;

using common::DeviceAddress;

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

const DeviceAddress kAddress0(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:01");
const DeviceAddress kAddrAlias0(DeviceAddress::Type::kBREDR, kAddress0.value());
const DeviceAddress kAddress1(DeviceAddress::Type::kLERandom,
                              "00:00:00:00:00:02");
const DeviceAddress kAddress2(DeviceAddress::Type::kBREDR, "00:00:00:00:00:03");

class LowEnergyConnectionManagerTest : public TestingBase {
 public:
  LowEnergyConnectionManagerTest() = default;
  ~LowEnergyConnectionManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    // Initialize with LE buffers only.
    TestingBase::InitializeACLDataChannel(
        hci::DataBufferInfo(),
        hci::DataBufferInfo(hci::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    dev_cache_ = std::make_unique<RemoteDeviceCache>();
    l2cap_ = data::testing::FakeDomain::Create();
    l2cap_->Initialize();

    connector_ = std::make_unique<hci::LowEnergyConnector>(
        transport(), &addr_delegate_, dispatcher(),
        fit::bind_member(
            this, &LowEnergyConnectionManagerTest::OnIncomingConnection));

    conn_mgr_ = std::make_unique<LowEnergyConnectionManager>(
        transport(), &addr_delegate_, connector_.get(), dev_cache_.get(),
        l2cap_, gatt::testing::FakeLayer::Create());

    test_device()->SetConnectionStateCallback(
        fit::bind_member(
            this, &LowEnergyConnectionManagerTest::OnConnectionStateChanged),
        dispatcher());
    StartTestDevice();
  }

  void TearDown() override {
    if (conn_mgr_)
      conn_mgr_ = nullptr;
    dev_cache_ = nullptr;

    l2cap_->ShutDown();
    l2cap_ = nullptr;

    TestingBase::TearDown();
  }

  // Deletes |conn_mgr_|.
  void DeleteConnMgr() { conn_mgr_ = nullptr; }

  RemoteDeviceCache* dev_cache() const { return dev_cache_.get(); }
  LowEnergyConnectionManager* conn_mgr() const { return conn_mgr_.get(); }
  data::testing::FakeDomain* fake_l2cap() const { return l2cap_.get(); }

  // Addresses of currently connected fake devices.
  using DeviceList = std::unordered_set<common::DeviceAddress>;
  const DeviceList& connected_devices() const { return connected_devices_; }

  // Addresses of devices with a canceled connection attempt.
  const DeviceList& canceled_devices() const { return canceled_devices_; }

  hci::ConnectionPtr MoveLastRemoteInitiated() {
    return std::move(last_remote_initiated_);
  }

 private:
  // Called by |connector_| when a new remote initiated connection is received.
  void OnIncomingConnection(hci::ConnectionHandle handle,
                            hci::Connection::Role role,
                            const common::DeviceAddress& peer_address,
                            const hci::LEConnectionParameters& conn_params) {
    common::DeviceAddress local_address(common::DeviceAddress::Type::kLEPublic,
                                        "03:02:01:01:02:03");

    // Create a production connection object that can interact with the fake
    // controller.
    last_remote_initiated_ = hci::Connection::CreateLE(
        handle, role, local_address, peer_address, conn_params, transport());
  }

  // Called by FakeController on connection events.
  void OnConnectionStateChanged(const common::DeviceAddress& address,
                                bool connected, bool canceled) {
    bt_log(SPEW, "gap-test",
           "OnConnectionStateChanged: %s connected: %s, canceled %s",
           address.ToString().c_str(), connected ? "true" : "false",
           canceled ? "true" : "false");
    if (canceled) {
      canceled_devices_.insert(address);
    } else if (connected) {
      ZX_DEBUG_ASSERT(connected_devices_.find(address) ==
                      connected_devices_.end());
      connected_devices_.insert(address);
    } else {
      ZX_DEBUG_ASSERT(connected_devices_.find(address) !=
                      connected_devices_.end());
      connected_devices_.erase(address);
    }
  }

  fbl::RefPtr<data::testing::FakeDomain> l2cap_;

  hci::FakeLocalAddressDelegate addr_delegate_;
  std::unique_ptr<RemoteDeviceCache> dev_cache_;
  std::unique_ptr<hci::LowEnergyConnector> connector_;
  std::unique_ptr<LowEnergyConnectionManager> conn_mgr_;

  // The most recent remote-initiated connection reported by |connector_|.
  hci::ConnectionPtr last_remote_initiated_;

  DeviceList connected_devices_;
  DeviceList canceled_devices_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionManagerTest);
};

using GAP_LowEnergyConnectionManagerTest = LowEnergyConnectionManagerTest;

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectUnknownDevice) {
  constexpr DeviceId kUnknownId(1);
  EXPECT_FALSE(conn_mgr()->Connect(kUnknownId, {}));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectClassicDevice) {
  auto* dev = dev_cache()->NewDevice(kAddress2, true);
  EXPECT_FALSE(conn_mgr()->Connect(dev->identifier(), {}));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectNonConnectableDevice) {
  auto* dev = dev_cache()->NewDevice(kAddress0, false);
  EXPECT_FALSE(conn_mgr()->Connect(dev->identifier(), {}));
}

// An error is received via the HCI Command cb_status event
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSingleDeviceErrorStatus) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_status(
      hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished,
            status.protocol_error());
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());
}

// LE Connection Complete event reports error
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSingleDeviceFailure) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_response(
      hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished,
            status.protocol_error());
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSingleDeviceTimeout) {
  constexpr zx::duration kTestRequestTimeout = zx::sec(20);

  auto* dev = dev_cache()->NewDevice(kAddress0, true);

  // We add no fake devices to cause the request to time out.

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  conn_mgr()->set_request_timeout_for_testing(kTestRequestTimeout);
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());

  RunLoopFor(kTestRequestTimeout);

  EXPECT_FALSE(status);
  EXPECT_EQ(common::HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());
}

// Tests that an entry in the cache does not expire while a connection attempt
// is pending.
TEST_F(GAP_LowEnergyConnectionManagerTest, PeerDoesNotExpireDuringTimeout) {
  // Set a connection timeout that is longer than the RemoteDeviceCache expiry
  // timeout.
  // TODO(BT-825): Consider configuring the cache timeout explicitly rather than
  // relying on the kCacheTimeout constant.
  constexpr zx::duration kTestRequestTimeout = kCacheTimeout + zx::sec(1);
  conn_mgr()->set_request_timeout_for_testing(kTestRequestTimeout);

  // Note: Use a random address so that the peer becomes temporary upon failure.
  auto* dev = dev_cache()->NewDevice(kAddress1, true);
  EXPECT_TRUE(dev->temporary());

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());
  EXPECT_FALSE(dev->temporary());

  RunLoopFor(kTestRequestTimeout);
  EXPECT_EQ(common::HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_EQ(dev, dev_cache()->FindDeviceByAddress(kAddress1));
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());
  EXPECT_TRUE(dev->temporary());
}

TEST_F(GAP_LowEnergyConnectionManagerTest,
       PeerDoesNotExpireDuringDelayedConnect) {
  // Make the connection resolve after a delay that is longer than the cache
  // timeout.
  constexpr zx::duration kConnectionDelay = kCacheTimeout + zx::sec(1);
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  settings.le_connection_delay = kConnectionDelay;
  test_device()->set_settings(settings);

  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto id = dev->identifier();
  EXPECT_TRUE(dev->temporary());

  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Make sure the connection request doesn't time out while waiting for a
  // response.
  conn_mgr()->set_request_timeout_for_testing(kConnectionDelay + zx::sec(1));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(common::HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);

    ASSERT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };
  EXPECT_TRUE(conn_mgr()->Connect(id, callback));
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());

  RunLoopFor(kConnectionDelay);
  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(status);

  // The device should not have expired during this time.
  dev = dev_cache()->FindDeviceByAddress(kAddress0);
  ASSERT_TRUE(dev);
  EXPECT_EQ(id, dev->identifier());
  EXPECT_TRUE(dev->connected());
  EXPECT_FALSE(dev->temporary());
}

// Successful connection to single device
TEST_F(GAP_LowEnergyConnectionManagerTest, ConnectSingleDevice) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  EXPECT_TRUE(dev->temporary());

  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(common::HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };

  EXPECT_TRUE(connected_devices().empty());
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kInitializing,
            dev->le()->connection_state());

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));

  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(conn_ref->active());
  EXPECT_EQ(dev->identifier(), conn_ref->device_identifier());
  EXPECT_FALSE(dev->temporary());
  EXPECT_EQ(RemoteDevice::ConnectionState::kConnected,
            dev->le()->connection_state());
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
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
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

  auto success_cb = [&conn_ref, &closed_cb, this](auto status,
                                                  auto cb_conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(cb_conn_ref);
    conn_ref = std::move(cb_conn_ref);
    conn_ref->set_closed_callback(std::move(closed_cb));
  };

  ASSERT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(conn_ref->active());

  // This will trigger the closed callback.
  EXPECT_TRUE(conn_mgr()->Disconnect(dev->identifier()));
  RunLoopUntilIdle();

  EXPECT_EQ(1, closed_count);
  EXPECT_TRUE(connected_devices().empty());
  EXPECT_FALSE(conn_ref);

  // The object should be deleted.
  EXPECT_TRUE(deleted);
}

TEST_F(GAP_LowEnergyConnectionManagerTest, ReleaseRef) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(common::HostError::kFailed);
  LowEnergyConnectionRefPtr conn_ref;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    EXPECT_TRUE(conn_ref->active());
  };

  EXPECT_TRUE(connected_devices().empty());
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, connected_devices().size());
  ASSERT_TRUE(dev->le());
  EXPECT_EQ(RemoteDevice::ConnectionState::kConnected,
            dev->le()->connection_state());

  ASSERT_TRUE(conn_ref);
  conn_ref = nullptr;

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_devices().empty());
  EXPECT_EQ(RemoteDevice::ConnectionState::kNotConnected,
            dev->le()->connection_state());
}

TEST_F(GAP_LowEnergyConnectionManagerTest,
       OneDeviceTwoPendingRequestsBothFail) {
  constexpr int kRequestCount = 2;

  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  fake_peer->set_connect_response(
      hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer));

  hci::Status statuses[kRequestCount];

  int cb_count = 0;
  auto callback = [&statuses, &cb_count](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    statuses[cb_count++] = cb_status;
  };

  for (int i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback))
        << "request count: " << i + 1;
  }

  RunLoopUntilIdle();

  ASSERT_EQ(kRequestCount, cb_count);
  for (int i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(statuses[i].is_protocol_error());
    EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished,
              statuses[i].protocol_error())
        << "request count: " << i + 1;
  }
}

TEST_F(GAP_LowEnergyConnectionManagerTest, OneDeviceManyPendingRequests) {
  constexpr size_t kRequestCount = 50;

  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  for (size_t i = 0; i < kRequestCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback))
        << "request count: " << i + 1;
  }

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));

  EXPECT_EQ(kRequestCount, conn_refs.size());
  for (size_t i = 0; i < kRequestCount; ++i) {
    ASSERT_TRUE(conn_refs[i]);
    EXPECT_TRUE(conn_refs[i]->active());
    EXPECT_EQ(dev->identifier(), conn_refs[i]->device_identifier());
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

  EXPECT_TRUE(connected_devices().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, AddRefAfterConnection) {
  constexpr size_t kRefCount = 50;

  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  auto fake_peer = std::make_unique<FakePeer>(kAddress0);
  test_device()->AddPeer(std::move(fake_peer));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));
  EXPECT_EQ(1u, conn_refs.size());

  // Add new references.
  for (size_t i = 1; i < kRefCount; ++i) {
    EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), callback))
        << "request count: " << i + 1;
    RunLoopUntilIdle();
  }

  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));
  EXPECT_EQ(kRefCount, conn_refs.size());

  // Disconnect.
  conn_refs.clear();

  RunLoopUntilIdle();

  EXPECT_TRUE(connected_devices().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, PendingRequestsOnTwoDevices) {
  auto* dev0 = dev_cache()->NewDevice(kAddress0, true);
  auto* dev1 = dev_cache()->NewDevice(kAddress1, true);

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress1));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto cb_status, auto conn_ref) {
    EXPECT_TRUE(conn_ref);
    EXPECT_TRUE(cb_status);
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev0->identifier(), callback));
  EXPECT_TRUE(conn_mgr()->Connect(dev1->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(2u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));
  EXPECT_EQ(1u, connected_devices().count(kAddress1));

  ASSERT_EQ(2u, conn_refs.size());
  ASSERT_TRUE(conn_refs[0]);
  ASSERT_TRUE(conn_refs[1]);
  EXPECT_EQ(dev0->identifier(), conn_refs[0]->device_identifier());
  EXPECT_EQ(dev1->identifier(), conn_refs[1]->device_identifier());

  // |dev1| should disconnect first.
  conn_refs[1] = nullptr;

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress0));

  conn_refs.clear();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_devices().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest,
       PendingRequestsOnTwoDevicesOneFails) {
  auto* dev0 = dev_cache()->NewDevice(kAddress0, true);
  auto* dev1 = dev_cache()->NewDevice(kAddress1, true);

  auto fake_peer0 = std::make_unique<FakePeer>(kAddress0);
  fake_peer0->set_connect_response(
      hci::StatusCode::kConnectionFailedToBeEstablished);
  test_device()->AddPeer(std::move(fake_peer0));
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress1));

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto callback = [&conn_refs](auto, auto conn_ref) {
    conn_refs.emplace_back(std::move(conn_ref));
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev0->identifier(), callback));
  EXPECT_TRUE(conn_mgr()->Connect(dev1->identifier(), callback));

  RunLoopUntilIdle();

  EXPECT_EQ(1u, connected_devices().size());
  EXPECT_EQ(1u, connected_devices().count(kAddress1));

  ASSERT_EQ(2u, conn_refs.size());
  EXPECT_FALSE(conn_refs[0]);
  ASSERT_TRUE(conn_refs[1]);
  EXPECT_EQ(dev1->identifier(), conn_refs[1]->device_identifier());

  // Both connections should disconnect.
  conn_refs.clear();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_devices().empty());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, Destructor) {
  auto* dev0 = dev_cache()->NewDevice(kAddress0, true);
  auto* dev1 = dev_cache()->NewDevice(kAddress1, true);

  // Connecting to this device will succeed.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // Connecting to this device will remain pending.
  auto pending_peer = std::make_unique<FakePeer>(kAddress1);
  pending_peer->set_force_pending_connect(true);
  test_device()->AddPeer(std::move(pending_peer));

  // Below we create one connection and one pending request to have at the time
  // of destruction.

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref, this](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    EXPECT_TRUE(status);

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev0->identifier(), success_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(conn_ref);
  bool conn_closed = false;
  conn_ref->set_closed_callback([&conn_closed] { conn_closed = true; });

  bool error_cb_called = false;
  auto error_cb = [&error_cb_called](auto status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    EXPECT_EQ(common::HostError::kFailed, status.error());
    error_cb_called = true;
  };

  // This will send an HCI command to the fake controller. We delete the
  // connection manager before a connection event gets received which should
  // cancel the connection.
  EXPECT_TRUE(conn_mgr()->Connect(dev1->identifier(), error_cb));
  DeleteConnMgr();

  RunLoopUntilIdle();

  EXPECT_TRUE(error_cb_called);
  EXPECT_TRUE(conn_closed);
  EXPECT_EQ(1u, canceled_devices().size());
  EXPECT_EQ(1u, canceled_devices().count(kAddress1));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectError) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // This should fail as |dev0| is not connected.
  EXPECT_FALSE(conn_mgr()->Disconnect(dev->identifier()));
}

TEST_F(GAP_LowEnergyConnectionManagerTest, Disconnect) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  int closed_count = 0;
  auto closed_cb = [&closed_count] { closed_count++; };

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto success_cb = [&conn_refs, &closed_cb, this](auto status, auto conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    conn_ref->set_closed_callback(closed_cb);
    conn_refs.push_back(std::move(conn_ref));
  };

  // Issue two connection refs.
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));

  RunLoopUntilIdle();

  ASSERT_EQ(2u, conn_refs.size());

  EXPECT_TRUE(conn_mgr()->Disconnect(dev->identifier()));

  RunLoopUntilIdle();

  EXPECT_EQ(2, closed_count);
  EXPECT_TRUE(connected_devices().empty());
  EXPECT_TRUE(canceled_devices().empty());
}

// Tests when a link is lost without explicitly disconnecting
TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectEvent) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  int closed_count = 0;
  auto closed_cb = [&closed_count, this] { closed_count++; };

  std::vector<LowEnergyConnectionRefPtr> conn_refs;
  auto success_cb = [&conn_refs, &closed_cb, this](auto status, auto conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(conn_ref);
    conn_ref->set_closed_callback(closed_cb);
    conn_refs.push_back(std::move(conn_ref));
  };

  // Issue two connection refs.
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));
  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));

  RunLoopUntilIdle();

  ASSERT_EQ(2u, conn_refs.size());

  // This makes FakeController send us HCI Disconnection Complete events.
  test_device()->Disconnect(kAddress0);

  RunLoopUntilIdle();

  EXPECT_EQ(2, closed_count);
}

TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectWhileRefPending) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref, this](auto status, auto cb_conn_ref) {
    EXPECT_TRUE(status);
    ASSERT_TRUE(cb_conn_ref);
    EXPECT_TRUE(cb_conn_ref->active());

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  auto ref_cb = [](auto status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    EXPECT_FALSE(status);
    EXPECT_EQ(common::HostError::kFailed, status.error());
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), ref_cb));

  // This should invalidate the ref that was bound to |ref_cb|.
  EXPECT_TRUE(conn_mgr()->Disconnect(dev->identifier()));

  RunLoopUntilIdle();
}

// This tests that a connection reference callback returns nullptr if a HCI
// Disconnection Complete event is received for the corresponding ACL link
// BEFORE the callback gets run.
TEST_F(GAP_LowEnergyConnectionManagerTest, DisconnectEventWhileRefPending) {
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  LowEnergyConnectionRefPtr conn_ref;
  auto success_cb = [&conn_ref, this](auto status, auto cb_conn_ref) {
    ASSERT_TRUE(cb_conn_ref);
    ASSERT_TRUE(status);
    EXPECT_TRUE(cb_conn_ref->active());

    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), success_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  // Request a new reference. Disconnect the link before the reference is
  // received.
  auto ref_cb = [](auto status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    EXPECT_FALSE(status);
    EXPECT_EQ(common::HostError::kFailed, status.error());
  };

  auto disconn_cb = [this, ref_cb, dev](auto) {
    // The link is gone but conn_mgr() hasn't updated the connection state yet.
    // The request to connect will attempt to add a new reference which will be
    // invalidated before |ref_cb| gets called.
    EXPECT_TRUE(conn_mgr()->Connect(dev->identifier(), ref_cb));
  };
  conn_mgr()->SetDisconnectCallbackForTesting(disconn_cb);

  test_device()->Disconnect(kAddress0);
  RunLoopUntilIdle();
}

// Listener receives remote initiated connection ref.
TEST_F(GAP_LowEnergyConnectionManagerTest, RegisterRemoteInitiatedLink) {
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // First create a fake incoming connection.
  test_device()->ConnectLowEnergy(kAddress0);

  RunLoopUntilIdle();

  auto link = MoveLastRemoteInitiated();
  ASSERT_TRUE(link);

  LowEnergyConnectionRefPtr conn_ref =
      conn_mgr()->RegisterRemoteInitiatedLink(std::move(link));
  ASSERT_TRUE(conn_ref);
  EXPECT_TRUE(conn_ref->active());

  // A RemoteDevice should now exist in the cache.
  auto* dev = dev_cache()->FindDeviceByAddress(kAddress0);
  ASSERT_TRUE(dev);
  EXPECT_EQ(dev->identifier(), conn_ref->device_identifier());
  EXPECT_TRUE(dev->connected());
  EXPECT_TRUE(dev->le()->connected());

  conn_ref = nullptr;

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_devices().empty());
}

// Listener receives remote initiated connection ref for a known device with the
// same BR/EDR address.
TEST_F(GAP_LowEnergyConnectionManagerTest,
       IncomingConnectionUpgradesKnownBrEdrDeviceToDualMode) {
  RemoteDevice* device = dev_cache()->NewDevice(kAddrAlias0, true);
  ASSERT_TRUE(device);
  ASSERT_EQ(device, dev_cache()->FindDeviceByAddress(kAddress0));
  ASSERT_EQ(TechnologyType::kClassic, device->technology());

  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));

  // First create a fake incoming connection.
  test_device()->ConnectLowEnergy(kAddress0);

  RunLoopUntilIdle();

  auto link = MoveLastRemoteInitiated();
  ASSERT_TRUE(link);

  LowEnergyConnectionRefPtr conn_ref =
      conn_mgr()->RegisterRemoteInitiatedLink(std::move(link));
  ASSERT_TRUE(conn_ref);

  EXPECT_EQ(device->identifier(), conn_ref->device_identifier());
  EXPECT_EQ(TechnologyType::kDualMode, device->technology());
}

// Tests that the master accepts the connection parameters that are sent from
// a fake slave and eventually applies them to the link.
TEST_F(GAP_LowEnergyConnectionManagerTest, L2CAPLEConnectionParameterUpdate) {
  // Set up a fake device and a connection over which to process the L2CAP
  // request.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  ASSERT_TRUE(dev);

  LowEnergyConnectionRefPtr conn_ref;
  auto conn_cb = [&conn_ref](const auto& dev_id, auto cr) {
    conn_ref = std::move(cr);
  };
  ASSERT_TRUE(conn_mgr()->Connect(dev->identifier(), conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);

  hci::LEPreferredConnectionParameters preferred(
      hci::kLEConnectionIntervalMin, hci::kLEConnectionIntervalMax,
      hci::kLEConnectionLatencyMax, hci::kLEConnectionSupervisionTimeoutMax);

  hci::LEConnectionParameters actual;
  bool fake_dev_cb_called = false;
  bool conn_params_cb_called = false;

  auto fake_dev_cb = [&actual, &fake_dev_cb_called](const auto& addr,
                                                    const auto& params) {
    fake_dev_cb_called = true;
    actual = params;
  };
  test_device()->SetLEConnectionParametersCallback(fake_dev_cb, dispatcher());

  auto conn_params_cb = [&conn_params_cb_called, &conn_ref](const auto& dev) {
    EXPECT_EQ(conn_ref->device_identifier(), dev.identifier());
    conn_params_cb_called = true;
  };
  conn_mgr()->SetConnectionParametersCallbackForTesting(conn_params_cb);

  fake_l2cap()->TriggerLEConnectionParameterUpdate(conn_ref->handle(),
                                                   preferred);

  RunLoopUntilIdle();

  EXPECT_TRUE(fake_dev_cb_called);
  ASSERT_TRUE(conn_params_cb_called);

  ASSERT_TRUE(dev->le());
  EXPECT_EQ(preferred, *dev->le()->preferred_connection_parameters());
  EXPECT_EQ(actual, *dev->le()->connection_parameters());
}

TEST_F(GAP_LowEnergyConnectionManagerTest, L2CAPSignalLinkError) {
  // Set up a fake device and a connection over which to process the L2CAP
  // request.
  test_device()->AddPeer(std::make_unique<FakePeer>(kAddress0));
  auto* dev = dev_cache()->NewDevice(kAddress0, true);
  ASSERT_TRUE(dev);

  fbl::RefPtr<l2cap::Channel> att_chan;
  auto l2cap_chan_cb = [&att_chan](auto chan) { att_chan = chan; };
  fake_l2cap()->set_channel_callback(l2cap_chan_cb);

  LowEnergyConnectionRefPtr conn_ref;
  auto conn_cb = [&conn_ref](const auto& dev_id, auto cr) {
    conn_ref = std::move(cr);
  };
  ASSERT_TRUE(conn_mgr()->Connect(dev->identifier(), conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(att_chan);
  ASSERT_EQ(1u, connected_devices().size());

  // Signaling a link error through the channel should disconnect the link.
  att_chan->SignalLinkError();

  RunLoopUntilIdle();
  EXPECT_TRUE(connected_devices().empty());
}

}  // namespace
}  // namespace gap
}  // namespace bt
