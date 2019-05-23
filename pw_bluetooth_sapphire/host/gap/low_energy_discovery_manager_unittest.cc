// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"

#include <fbl/macros.h>
#include <zircon/assert.h>

#include <unordered_set>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {
namespace gap {
namespace {

using bt::testing::FakeController;
using bt::testing::FakePeer;

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

const DeviceAddress kAddress0(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:00");
const DeviceAddress kAddrAlias0(DeviceAddress::Type::kBREDR, kAddress0.value());
const DeviceAddress kAddress1(DeviceAddress::Type::kLERandom,
                              "00:00:00:00:00:01");
const DeviceAddress kAddress2(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:02");
const DeviceAddress kAddress3(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:03");
const DeviceAddress kAddress4(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:04");
const DeviceAddress kAddress5(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:05");

constexpr zx::duration kTestScanPeriod = zx::sec(10);

class LowEnergyDiscoveryManagerTest : public TestingBase {
 public:
  LowEnergyDiscoveryManagerTest() = default;
  ~LowEnergyDiscoveryManagerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    scan_enabled_ = false;

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    // TODO(armansito): Now that the hci::LowEnergyScanner is injected into
    // |discovery_manager_| rather than constructed by it, a fake implementation
    // could be injected directly. Consider providing fake behavior here in this
    // harness rather than using a FakeController.
    scanner_ = std::make_unique<hci::LegacyLowEnergyScanner>(
        &fake_address_delegate_, transport(), dispatcher());
    discovery_manager_ = std::make_unique<LowEnergyDiscoveryManager>(
        transport(), scanner_.get(), &peer_cache_);
    test_device()->SetScanStateCallback(
        std::bind(&LowEnergyDiscoveryManagerTest::OnScanStateChanged, this,
                  std::placeholders::_1),
        dispatcher());
    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    if (discovery_manager_) {
      discovery_manager_ = nullptr;
    }
    test_device()->Stop();
    TestingBase::TearDown();
  }

 protected:
  LowEnergyDiscoveryManager* discovery_manager() const {
    return discovery_manager_.get();
  }

  // Deletes |discovery_manager_|.
  void DeleteDiscoveryManager() { discovery_manager_ = nullptr; }

  PeerCache* peer_cache() { return &peer_cache_; }

  // Returns the last reported scan state of the FakeController.
  bool scan_enabled() const { return scan_enabled_; }

  // The scan states that the FakeController has transitioned through.
  const std::vector<bool> scan_states() const { return scan_states_; }

  // Sets a callback that will run when the scan state transitions |count|
  // times.
  void set_scan_state_handler(size_t count, fit::closure callback) {
    scan_state_callbacks_[count] = std::move(callback);
  }

  // Called by FakeController when the scan state changes.
  void OnScanStateChanged(bool enabled) {
    bt_log(TRACE, "gap-test", "FakeController scan state: %s",
           enabled ? "enabled" : "disabled");
    scan_enabled_ = enabled;
    scan_states_.push_back(enabled);

    auto iter = scan_state_callbacks_.find(scan_states_.size());
    if (iter != scan_state_callbacks_.end()) {
      iter->second();
    }
  }

  // Registers the following fake peers with the FakeController:
  //
  // Peer 0:
  //   - Connectable, not scannable;
  //   - General discoverable;
  //   - UUIDs: 0x180d, 0x180f;
  //   - has name: "Device 0"
  //
  // Peer 1:
  //   - Connectable, not scannable;
  //   - Limited discoverable;
  //   - UUIDs: 0x180d;
  //   - has name: "Device 1"
  //
  // Peer 2:
  //   - Not connectable, not scannable;
  //   - General discoverable;
  //   - UUIDs: none;
  //   - has name: "Device 2"
  //
  // Peer 3:
  //   - Not discoverable;
  void AddFakePeers() {
    // Peer 0
    const auto kAdvData0 = CreateStaticByteBuffer(
        // Flags
        0x02, 0x01, 0x02,

        // Complete 16-bit service UUIDs
        0x05, 0x03, 0x0d, 0x18, 0x0f, 0x18,

        // Complete local name
        0x09, 0x09, 'D', 'e', 'v', 'i', 'c', 'e', ' ', '0');
    auto fake_peer = std::make_unique<FakePeer>(kAddress0, true, true);
    fake_peer->SetAdvertisingData(kAdvData0);
    test_device()->AddPeer(std::move(fake_peer));

    // Peer 1
    const auto kAdvData1 = CreateStaticByteBuffer(
        // Flags
        0x02, 0x01, 0x01,

        // Complete 16-bit service UUIDs
        0x03, 0x03, 0x0d, 0x18);
    fake_peer = std::make_unique<FakePeer>(kAddress1, true, true);
    fake_peer->SetAdvertisingData(kAdvData1);
    test_device()->AddPeer(std::move(fake_peer));

    // Peer 2
    const auto kAdvData2 = CreateStaticByteBuffer(
        // Flags
        0x02, 0x01, 0x02,

        // Complete local name
        0x09, 0x09, 'D', 'e', 'v', 'i', 'c', 'e', ' ', '2');
    fake_peer = std::make_unique<FakePeer>(kAddress2, false, false);
    fake_peer->SetAdvertisingData(kAdvData2);
    test_device()->AddPeer(std::move(fake_peer));

    // Peer 3
    const auto kAdvData3 = CreateStaticByteBuffer(
        // Flags
        0x02, 0x01, 0x00,

        // Complete local name
        0x09, 0x09, 'D', 'e', 'v', 'i', 'c', 'e', ' ', '3');
    fake_peer = std::make_unique<FakePeer>(kAddress3, false, false);
    fake_peer->SetAdvertisingData(kAdvData3);
    test_device()->AddPeer(std::move(fake_peer));
  }

  // Creates and returns a discovery session.
  std::unique_ptr<LowEnergyDiscoverySession> StartDiscoverySession() {
    std::unique_ptr<LowEnergyDiscoverySession> session;
    discovery_manager()->StartDiscovery([&](auto cb_session) {
      ZX_DEBUG_ASSERT(cb_session);
      session = std::move(cb_session);
    });

    RunLoopUntilIdle();
    ZX_DEBUG_ASSERT(session);
    return session;
  }

 private:
  PeerCache peer_cache_;
  hci::FakeLocalAddressDelegate fake_address_delegate_;
  std::unique_ptr<hci::LegacyLowEnergyScanner> scanner_;
  std::unique_ptr<LowEnergyDiscoveryManager> discovery_manager_;

  bool scan_enabled_;
  std::vector<bool> scan_states_;
  std::unordered_map<size_t, fit::closure> scan_state_callbacks_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyDiscoveryManagerTest);
};

using GAP_LowEnergyDiscoveryManagerTest = LowEnergyDiscoveryManagerTest;

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryAndStop) {
  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  RunLoopUntilIdle();

  // The test fixture will be notified of the change in scan state before we
  // receive the session.
  EXPECT_TRUE(scan_enabled());
  RunLoopUntilIdle();

  ASSERT_TRUE(session);
  EXPECT_TRUE(session->active());

  session->Stop();
  EXPECT_FALSE(session->active());

  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryAndStopByDeleting) {
  // Start discovery but don't acquire ownership of the received session. This
  // should immediately terminate the session.
  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  RunLoopUntilIdle();

  // The test fixture will be notified of the change in scan state before we
  // receive the session.
  EXPECT_TRUE(scan_enabled());
  RunLoopUntilIdle();

  ASSERT_TRUE(session);
  EXPECT_TRUE(session->active());

  session = nullptr;

  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, Destructor) {
  // Start discovery with a session, delete the manager and ensure that the
  // session is inactive with the error callback called.
  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  RunLoopUntilIdle();

  EXPECT_TRUE(scan_enabled());

  ASSERT_TRUE(session);
  EXPECT_TRUE(session->active());

  size_t num_errors = 0u;
  session->set_error_callback([&num_errors]() { num_errors++; });

  EXPECT_EQ(0u, num_errors);
  DeleteDiscoveryManager();
  EXPECT_EQ(1u, num_errors);
  EXPECT_FALSE(session->active());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryAndStopInCallback) {
  // Start discovery but don't acquire ownership of the received session. This
  // should terminate the session when |session| goes out of scope.
  discovery_manager()->StartDiscovery([](auto session) {});

  RunLoopUntilIdle();
  ASSERT_EQ(2u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryFailure) {
  test_device()->SetDefaultResponseStatus(hci::kLESetScanEnable,
                                          hci::StatusCode::kCommandDisallowed);

  // |session| should contain nullptr.
  discovery_manager()->StartDiscovery(
      [](auto session) { EXPECT_FALSE(session); });

  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryWhileScanning) {
  std::vector<std::unique_ptr<LowEnergyDiscoverySession>> sessions;

  constexpr size_t kExpectedSessionCount = 5;
  size_t cb_count = 0u;
  auto cb = [&cb_count, &sessions](auto session) {
    sessions.push_back(std::move(session));
    cb_count++;
  };

  discovery_manager()->StartDiscovery(cb);

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_EQ(1u, sessions.size());

  // Add the rest of the sessions. These are expected to succeed immediately but
  // the callbacks should be called asynchronously.
  for (size_t i = 1u; i < kExpectedSessionCount; i++) {
    discovery_manager()->StartDiscovery(cb);
  }

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_EQ(kExpectedSessionCount, sessions.size());

  // Remove one session from the list. Scan should continue.
  sessions.pop_back();
  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // Remove all but one session from the list. Scan should continue.
  sessions.erase(sessions.begin() + 1, sessions.end());
  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_EQ(1u, sessions.size());

  // Remove the last session.
  sessions.clear();
  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryWhilePendingStart) {
  std::vector<std::unique_ptr<LowEnergyDiscoverySession>> sessions;

  constexpr size_t kExpectedSessionCount = 5;
  size_t cb_count = 0u;
  auto cb = [&cb_count, &sessions](auto session) {
    sessions.push_back(std::move(session));
    cb_count++;
  };

  for (size_t i = 0u; i < kExpectedSessionCount; i++) {
    discovery_manager()->StartDiscovery(cb);
  }

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_EQ(kExpectedSessionCount, sessions.size());

  // Remove all sessions. This should stop the scan.
  sessions.clear();
  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       StartDiscoveryWhilePendingStartAndStopInCallback) {
  constexpr size_t kExpectedSessionCount = 5;
  size_t cb_count = 0u;
  std::unique_ptr<LowEnergyDiscoverySession> session;
  auto cb = [&cb_count, &session](auto cb_session) {
    cb_count++;
    if (cb_count == kExpectedSessionCount) {
      // Hold on to only the last session object. The rest should get deleted
      // within the callback.
      session = std::move(cb_session);
    }
  };

  for (size_t i = 0u; i < kExpectedSessionCount; i++) {
    discovery_manager()->StartDiscovery(cb);
  }

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_TRUE(session);

  RunLoopUntilIdle();
  EXPECT_EQ(kExpectedSessionCount, cb_count);
  EXPECT_TRUE(scan_enabled());

  // Deleting the only remaning session should stop the scan.
  session = nullptr;
  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryWhilePendingStop) {
  std::unique_ptr<LowEnergyDiscoverySession> session;

  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  EXPECT_TRUE(session);

  // Stop the session. This should issue a request to stop the ongoing scan but
  // the request will remain pending until we run the message loop.
  session = nullptr;

  // Request a new session. The discovery manager should restart the scan after
  // the ongoing one stops.
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  // Discovery should stop and start again.
  RunLoopUntilIdle();
  ASSERT_EQ(3u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryFailureManyPending) {
  test_device()->SetDefaultResponseStatus(hci::kLESetScanEnable,
                                          hci::StatusCode::kCommandDisallowed);

  constexpr size_t kExpectedSessionCount = 5;
  size_t cb_count = 0u;
  auto cb = [&cb_count](auto session) {
    // |session| should contain nullptr as the request will fail.
    EXPECT_FALSE(session);
    cb_count++;
  };

  for (size_t i = 0u; i < kExpectedSessionCount; i++) {
    discovery_manager()->StartDiscovery(cb);
  }

  RunLoopUntilIdle();
  EXPECT_FALSE(scan_enabled());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, ScanPeriodRestart) {
  constexpr size_t kNumScanStates = 3;

  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  // We should observe the scan state become enabled -> disabled -> enabled.
  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // End the scan period.
  RunLoopFor(kTestScanPeriod);
  ASSERT_EQ(kNumScanStates, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, ScanPeriodRestartFailure) {
  constexpr size_t kNumScanStates = 2;

  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unique_ptr<LowEnergyDiscoverySession> session;
  bool session_error = false;
  discovery_manager()->StartDiscovery([&](auto cb_session) {
    session = std::move(cb_session);
    session->set_error_callback(
        [&session_error] { session_error = true; });
  });

  // The controller will fail to restart scanning after scanning stops at the
  // end of the period. The scan state will transition twice (-> enabled ->
  // disabled).
  set_scan_state_handler(kNumScanStates, [this] {
    test_device()->SetDefaultResponseStatus(
        hci::kLESetScanEnable, hci::StatusCode::kCommandDisallowed);
  });

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // End the scan period. The scan should not restart.
  RunLoopFor(kTestScanPeriod);

  ASSERT_EQ(kNumScanStates, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(session_error);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, ScanPeriodRestartRemoveSession) {
  constexpr size_t kNumScanStates = 4;

  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  // We should observe 3 scan state transitions (-> enabled -> disabled ->
  // enabled).
  set_scan_state_handler(kNumScanStates - 1, [this, &session] {
    ASSERT_TRUE(session);
    EXPECT_TRUE(scan_enabled());

    // At this point the fake controller has updated its state but the discovery
    // manager has not processed the restarted scan. We should be able to remove
    // the current session and the state should ultimately become disabled.
    session->Stop();
  });

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // End the scan period.
  RunLoopFor(kTestScanPeriod);

  ASSERT_EQ(kNumScanStates, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
  EXPECT_FALSE(scan_states()[3]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       ScanPeriodRemoveSessionDuringRestart) {
  constexpr size_t kNumScanStates = 2;

  // Set a very short scan period for the sake of the test.
  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery(
      [&session](auto cb_session) { session = std::move(cb_session); });

  // The controller will fail to restart scanning after scanning stops at the
  // end of the period. The scan state will transition twice (-> enabled ->
  // disabled).
  set_scan_state_handler(kNumScanStates, [this, &session] {
    ASSERT_TRUE(session);
    EXPECT_FALSE(scan_enabled());

    // Stop the session before the discovery manager processes the event. It
    // should detect this and discontinue the scan.
    session->Stop();
  });

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // End the scan period.
  RunLoopFor(kTestScanPeriod);

  ASSERT_EQ(kNumScanStates, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       ScanPeriodRestartRemoveAndAddSession) {
  constexpr size_t kNumScanPeriodRestartStates = 3;
  constexpr size_t kTotalNumStates = 5;

  // Set a very short scan period for the sake of the test.
  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unique_ptr<LowEnergyDiscoverySession> session;
  auto cb = [&session](auto cb_session) { session = std::move(cb_session); };
  discovery_manager()->StartDiscovery(cb);

  // We should observe 3 scan state transitions (-> enabled -> disabled ->
  // enabled).
  set_scan_state_handler(kNumScanPeriodRestartStates, [this, &session, cb] {
    ASSERT_TRUE(session);
    EXPECT_TRUE(scan_enabled());

    // At this point the fake controller has updated its state but the discovery
    // manager has not processed the restarted scan. We should be able to remove
    // the current session and create a new one and the state should update
    // accordingly.
    session->Stop();
    discovery_manager()->StartDiscovery(cb);
  });

  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());

  // End the scan period.
  RunLoopFor(kTestScanPeriod);

  // Scan should have been disabled and re-enabled.
  ASSERT_EQ(kTotalNumStates, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryWithFilters) {
  AddFakePeers();

  std::vector<std::unique_ptr<LowEnergyDiscoverySession>> sessions;

  // Set a short scan period so that we that we process events for multiple scan
  // periods during the test.
  discovery_manager()->set_scan_period(zx::msec(200));

  // Session 0 is interested in performing general discovery.
  std::unordered_set<DeviceAddress> peers_session0;
  LowEnergyDiscoverySession::PeerFoundCallback result_cb =
      [&peers_session0](const auto& peer) {
        peers_session0.insert(peer.address());
      };
  sessions.push_back(StartDiscoverySession());
  sessions[0]->filter()->SetGeneralDiscoveryFlags();
  sessions[0]->SetResultCallback(std::move(result_cb));

  // Session 1 is interested in performing limited discovery.
  std::unordered_set<DeviceAddress> peers_session1;
  result_cb = [&peers_session1](const auto& peer) {
    peers_session1.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[1]->filter()->set_flags(
      static_cast<uint8_t>(AdvFlag::kLELimitedDiscoverableMode));
  sessions[1]->SetResultCallback(std::move(result_cb));

  // Session 2 is interested in peers with UUID 0x180d.
  std::unordered_set<DeviceAddress> peers_session2;
  result_cb = [&peers_session2](const auto& peer) {
    peers_session2.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());

  uint16_t uuid = 0x180d;
  sessions[2]->filter()->set_service_uuids({UUID(uuid)});
  sessions[2]->SetResultCallback(std::move(result_cb));

  // Session 3 is interested in peers whose names contain "Device".
  std::unordered_set<DeviceAddress> peers_session3;
  result_cb = [&peers_session3](const auto& peer) {
    peers_session3.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[3]->filter()->set_name_substring("Device");
  sessions[3]->SetResultCallback(std::move(result_cb));

  // Session 4 is interested in non-connectable peers.
  std::unordered_set<DeviceAddress> peers_session4;
  result_cb = [&peers_session4](const auto& peer) {
    peers_session4.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[4]->filter()->set_connectable(false);
  sessions[4]->SetResultCallback(std::move(result_cb));

  RunLoopUntilIdle();

  EXPECT_EQ(5u, sessions.size());

#define EXPECT_CONTAINS(addr, dev_list) \
  EXPECT_TRUE(dev_list.find(addr) != dev_list.end())
  // At this point all sessions should have processed all peers at least once.

  // Session 0: Should have seen all peers except for peer 3, which is
  // non-discoverable.
  EXPECT_EQ(3u, peers_session0.size());
  EXPECT_CONTAINS(kAddress0, peers_session0);
  EXPECT_CONTAINS(kAddress1, peers_session0);
  EXPECT_CONTAINS(kAddress2, peers_session0);

  // Session 1: Should have only seen peer 1.
  EXPECT_EQ(1u, peers_session1.size());
  EXPECT_CONTAINS(kAddress1, peers_session1);

  // Session 2: Should have only seen peers 0 and 1
  EXPECT_EQ(2u, peers_session2.size());
  EXPECT_CONTAINS(kAddress0, peers_session2);
  EXPECT_CONTAINS(kAddress1, peers_session2);

  // Session 3: Should have only seen peers 0, 2, and 3
  EXPECT_EQ(3u, peers_session3.size());
  EXPECT_CONTAINS(kAddress0, peers_session3);
  EXPECT_CONTAINS(kAddress2, peers_session3);
  EXPECT_CONTAINS(kAddress3, peers_session3);

  // Session 4: Should have seen peers 2 and 3
  EXPECT_EQ(2u, peers_session4.size());
  EXPECT_CONTAINS(kAddress2, peers_session4);
  EXPECT_CONTAINS(kAddress3, peers_session4);

#undef EXPECT_CONTAINS
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       StartDiscoveryWithFiltersCachedPeerNotifications) {
  AddFakePeers();

  std::vector<std::unique_ptr<LowEnergyDiscoverySession>> sessions;

  // Set a long scan period to make sure that the FakeController sends
  // advertising reports only once.
  discovery_manager()->set_scan_period(zx::sec(20));

  // Session 0 is interested in performing general discovery.
  std::unordered_set<DeviceAddress> peers_session0;
  LowEnergyDiscoverySession::PeerFoundCallback result_cb =
      [&peers_session0](const auto& peer) {
        peers_session0.insert(peer.address());
      };
  sessions.push_back(StartDiscoverySession());
  sessions[0]->filter()->SetGeneralDiscoveryFlags();
  sessions[0]->SetResultCallback(std::move(result_cb));

  RunLoopUntilIdle();
  ASSERT_EQ(3u, peers_session0.size());

  // Session 1 is interested in performing limited discovery.
  std::unordered_set<DeviceAddress> peers_session1;
  result_cb = [&peers_session1](const auto& peer) {
    peers_session1.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[1]->filter()->set_flags(
      static_cast<uint8_t>(AdvFlag::kLELimitedDiscoverableMode));
  sessions[1]->SetResultCallback(std::move(result_cb));

  // Session 2 is interested in peers with UUID 0x180d.
  std::unordered_set<DeviceAddress> peers_session2;
  result_cb = [&peers_session2](const auto& peer) {
    peers_session2.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());

  uint16_t uuid = 0x180d;
  sessions[2]->filter()->set_service_uuids({UUID(uuid)});
  sessions[2]->SetResultCallback(std::move(result_cb));

  // Session 3 is interested in peers whose names contain "Device".
  std::unordered_set<DeviceAddress> peers_session3;
  result_cb = [&peers_session3](const auto& peer) {
    peers_session3.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[3]->filter()->set_name_substring("Device");
  sessions[3]->SetResultCallback(std::move(result_cb));

  // Session 4 is interested in non-connectable peers.
  std::unordered_set<DeviceAddress> peers_session4;
  result_cb = [&peers_session4](const auto& peer) {
    peers_session4.insert(peer.address());
  };
  sessions.push_back(StartDiscoverySession());
  sessions[4]->filter()->set_connectable(false);
  sessions[4]->SetResultCallback(std::move(result_cb));

  EXPECT_EQ(5u, sessions.size());

#define EXPECT_CONTAINS(addr, dev_list) \
  EXPECT_TRUE(dev_list.find(addr) != dev_list.end())
  // At this point all sessions should have processed all peers at least once
  // without running the message loop; results for Sessions 1, 2, 3, and 4
  // should have come from the cache.

  // Session 0: Should have seen all peers except for peer 3, which is
  // non-discoverable.
  EXPECT_EQ(3u, peers_session0.size());
  EXPECT_CONTAINS(kAddress0, peers_session0);
  EXPECT_CONTAINS(kAddress1, peers_session0);
  EXPECT_CONTAINS(kAddress2, peers_session0);

  // Session 1: Should have only seen peer 1.
  EXPECT_EQ(1u, peers_session1.size());
  EXPECT_CONTAINS(kAddress1, peers_session1);

  // Session 2: Should have only seen peers 0 and 1
  EXPECT_EQ(2u, peers_session2.size());
  EXPECT_CONTAINS(kAddress0, peers_session2);
  EXPECT_CONTAINS(kAddress1, peers_session2);

  // Session 3: Should have only seen peers 0, 2, and 3
  EXPECT_EQ(3u, peers_session3.size());
  EXPECT_CONTAINS(kAddress0, peers_session3);
  EXPECT_CONTAINS(kAddress2, peers_session3);
  EXPECT_CONTAINS(kAddress3, peers_session3);

  // Session 4: Should have seen peers 2 and 3
  EXPECT_EQ(2u, peers_session4.size());
  EXPECT_CONTAINS(kAddress2, peers_session4);
  EXPECT_CONTAINS(kAddress3, peers_session4);

#undef EXPECT_CONTAINS
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, DirectedConnectableEvent) {
  auto fake_peer = std::make_unique<FakePeer>(kAddress0, true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  int count = 0;
  discovery_manager()->set_bonded_peer_connectable_callback(
      [&](const auto&) { count++; });
  discovery_manager()->set_scan_period(kTestScanPeriod);

  // Start discovery. Advertisements from the peer should be ignored since the
  // peer is not bonded.
  auto session = StartDiscoverySession();
  RunLoopUntilIdle();
  ASSERT_TRUE(session);
  EXPECT_EQ(0, count);

  // Mark the peer as bonded.
  constexpr PeerId kPeerId(1);
  sm::PairingData pdata;
  pdata.ltk = sm::LTK();
  peer_cache()->AddBondedPeer(BondingData{kPeerId, kAddress0, {}, pdata, {}});
  EXPECT_EQ(1u, peer_cache()->count());

  // Advance to the next scan period. We should receive a new notification.
  RunLoopFor(kTestScanPeriod);
  EXPECT_EQ(1, count);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       ScanResuiltUpgradesKnownBrEdrPeerToDualMode) {
  Peer* peer = peer_cache()->NewPeer(kAddrAlias0, true);
  ASSERT_TRUE(peer);
  ASSERT_EQ(peer, peer_cache()->FindByAddress(kAddress0));
  ASSERT_EQ(TechnologyType::kClassic, peer->technology());

  AddFakePeers();

  discovery_manager()->set_scan_period(kTestScanPeriod);

  std::unordered_set<DeviceAddress> addresses_found;
  LowEnergyDiscoverySession::PeerFoundCallback result_cb =
      [&addresses_found](const auto& peer) {
        addresses_found.insert(peer.address());
      };
  auto session = StartDiscoverySession();
  session->filter()->SetGeneralDiscoveryFlags();
  session->SetResultCallback(std::move(result_cb));

  RunLoopUntilIdle();

  ASSERT_EQ(3u, addresses_found.size());
  EXPECT_TRUE(addresses_found.find(kAddrAlias0) != addresses_found.end());
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, EnableBackgroundScan) {
  ASSERT_FALSE(test_device()->le_scan_state().enabled);

  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();

  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEScanType::kPassive,
            test_device()->le_scan_state().scan_type);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, DisableBackgroundScan) {
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_scan_state().enabled);

  discovery_manager()->EnableBackgroundScan(false);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       EnableAndDisableBackgroundScanQuickly) {
  ASSERT_FALSE(test_device()->le_scan_state().enabled);

  discovery_manager()->EnableBackgroundScan(true);
  discovery_manager()->EnableBackgroundScan(false);
  RunLoopUntilIdle();

  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(2u, scan_states().size());

  // This should not result in a request to stop scan.
  discovery_manager()->EnableBackgroundScan(true);
  discovery_manager()->EnableBackgroundScan(false);
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  EXPECT_EQ(3u, scan_states().size());

  EXPECT_TRUE(test_device()->le_scan_state().enabled);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, EnableBackgroundScanDuringDiscovery) {
  auto session = StartDiscoverySession();
  ASSERT_TRUE(session);
  ASSERT_TRUE(test_device()->le_scan_state().enabled);
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);

  // The scan state should transition to enabled.
  ASSERT_EQ(1u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);

  // Enabling background scans should not disable the active scan.
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(1u, scan_states().size());

  // Stopping the discovery session should fall back to passive scan.
  session = nullptr;
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEScanType::kPassive,
            test_device()->le_scan_state().scan_type);

  // We expect the following state transitions: -> disabled -> enabled
  ASSERT_EQ(3u, scan_states().size());
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       DisableBackgroundScanDuringDiscovery) {
  auto session = StartDiscoverySession();
  ASSERT_TRUE(session);
  ASSERT_TRUE(test_device()->le_scan_state().enabled);
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);

  // The scan state should transition to enabled.
  ASSERT_EQ(1u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);

  // Enabling background scans should not disable the active scan.
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(1u, scan_states().size());

  // Disabling the background scan should not disable the active scan.
  discovery_manager()->EnableBackgroundScan(false);
  RunLoopUntilIdle();
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(1u, scan_states().size());

  // Stopping the discovery session should stop scans.
  session = nullptr;
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  // We expect the following state transitions: -> disabled
  ASSERT_EQ(2u, scan_states().size());
  EXPECT_FALSE(scan_states()[1]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, StartDiscoveryDuringBackgroundScan) {
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_scan_state().enabled);
  ASSERT_EQ(hci::LEScanType::kPassive,
            test_device()->le_scan_state().scan_type);

  // The scan state should transition to enabled.
  ASSERT_EQ(1u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);

  // Starting discovery should turn off the passive scan and initiate an active
  // scan.
  auto session = StartDiscoverySession();
  EXPECT_TRUE(session);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);

  // We expect the following state transitions: -> disabled -> enabled
  ASSERT_EQ(3u, scan_states().size());
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       StartDiscoveryWhileEnablingBackgroundScan) {
  discovery_manager()->EnableBackgroundScan(true);
  std::unique_ptr<LowEnergyDiscoverySession> session;
  discovery_manager()->StartDiscovery([&](auto cb_session) {
    ZX_DEBUG_ASSERT(cb_session);
    session = std::move(cb_session);
  });
  ASSERT_FALSE(session);

  // Scan should not be enabled yet.
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scan_states().empty());

  // Process all the requests. We should observe multiple state transitions:
  // -> enabled (passive) -> disabled -> enabled (active)
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_scan_state().enabled);
  ASSERT_EQ(hci::LEScanType::kActive, test_device()->le_scan_state().scan_type);
  ASSERT_EQ(3u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest,
       BackgroundScanOnlyHandlesEventsFromBondedDevices) {
  PeerId kBondedPeerId1(1);
  PeerId kBondedPeerId2(2);

  // kAddress0 and kAddress1 are in undirected connectable mode.
  AddFakePeers();

  // Add two devices that are in directed connectable mode.
  auto fake_peer = std::make_unique<FakePeer>(kAddress4, true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  fake_peer = std::make_unique<FakePeer>(kAddress5, true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  // Mark one directed and one undirected connectable device as bonded. We
  // expect advertisements from all other devices to get ignored.
  sm::PairingData pdata;
  pdata.ltk = sm::LTK();
  peer_cache()->AddBondedPeer(BondingData{kBondedPeerId1, kAddress0, {}, pdata,
                                          {}});
  peer_cache()->AddBondedPeer(BondingData{kBondedPeerId2, kAddress4, {}, pdata,
                                          {}});
  EXPECT_EQ(2u, peer_cache()->count());

  int count = 0;
  discovery_manager()->set_bonded_peer_connectable_callback(
      [&](const auto& id) {
        count++;
        EXPECT_TRUE(id == kBondedPeerId1 || id == kBondedPeerId2)
            << id.ToString();
      });
  discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  EXPECT_EQ(2, count);

  // No new remote peer cache entries should have been created.
  EXPECT_EQ(2u, peer_cache()->count());
}

TEST_F(GAP_LowEnergyDiscoveryManagerTest, BackgroundScanPeriodRestart) {
  discovery_manager()->set_scan_period(kTestScanPeriod);
  discovery_manager()->EnableBackgroundScan(true);

  // The scan state should transition to enabled.
  RunLoopUntilIdle();
  EXPECT_TRUE(scan_enabled());
  ASSERT_EQ(1u, scan_states().size());
  EXPECT_TRUE(scan_states()[0]);

  // End the scan period by advancing time.
  RunLoopFor(kTestScanPeriod);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEScanType::kPassive,
            test_device()->le_scan_state().scan_type);

  // We expect the following state transitions due to the timeout:
  // -> disabled -> enabled.
  ASSERT_EQ(3u, scan_states().size());
  EXPECT_FALSE(scan_states()[1]);
  EXPECT_TRUE(scan_states()[2]);
}

}  // namespace
}  // namespace gap
}  // namespace bt
