// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"

#include <lib/async/cpp/task.h>
#include <lib/zx/channel.h>

#include <memory>

#include "low_energy_address_manager.h"
#include "low_energy_advertising_manager.h"
#include "low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {
namespace gap {
namespace {

using testing::FakeController;
using testing::FakePeer;
using TestingBase = testing::FakeControllerTest<FakeController>;

const DeviceAddress kTestAddr(DeviceAddress::Type::kLEPublic,
                              "00:00:00:00:00:01");
const DeviceAddress kTestAddr2(DeviceAddress::Type::kLEPublic,
                               "00:00:00:00:00:02");

class AdapterTest : public TestingBase {
 public:
  AdapterTest() = default;
  ~AdapterTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();
    transport_closed_called_ = false;

    auto data_domain = data::testing::FakeDomain::Create();
    data_domain->Initialize();
    adapter_ = std::make_unique<Adapter>(transport(), std::move(data_domain),
                                         gatt::testing::FakeLayer::Create());
    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    if (adapter_->IsInitialized()) {
      adapter_->ShutDown();
    }

    adapter_ = nullptr;
    TestingBase::TearDown();
  }

  void InitializeAdapter(Adapter::InitializeCallback callback) {
    adapter_->Initialize(std::move(callback),
                         [this] { transport_closed_called_ = true; });
    RunLoopUntilIdle();
  }

 protected:
  bool transport_closed_called() const { return transport_closed_called_; }

  Adapter* adapter() const { return adapter_.get(); }

 private:
  bool transport_closed_called_;
  std::unique_ptr<Adapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdapterTest);
};

using GAP_AdapterTest = AdapterTest;

TEST_F(GAP_AdapterTest, InitializeFailureNoFeaturesSupported) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // The controller supports nothing.
  InitializeAdapter(std::move(init_cb));
  EXPECT_FALSE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_FALSE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, InitializeFailureNoBufferInfo) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // Enable LE support.
  FakeController::Settings settings;
  settings.lmp_features_page0 |=
      static_cast<uint64_t>(hci::LMPFeature::kLESupported);
  test_device()->set_settings(settings);

  InitializeAdapter(std::move(init_cb));
  EXPECT_FALSE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_FALSE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, InitializeNoBREDR) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // Enable LE support, disable BR/EDR
  FakeController::Settings settings;
  settings.lmp_features_page0 |=
      static_cast<uint64_t>(hci::LMPFeature::kLESupported);
  settings.lmp_features_page0 |=
      static_cast<uint64_t>(hci::LMPFeature::kBREDRNotSupported);
  settings.le_acl_data_packet_length = 5;
  settings.le_total_num_acl_data_packets = 1;
  test_device()->set_settings(settings);

  InitializeAdapter(std::move(init_cb));
  EXPECT_TRUE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_TRUE(adapter()->state().IsLowEnergySupported());
  EXPECT_FALSE(adapter()->state().IsBREDRSupported());
  EXPECT_EQ(TechnologyType::kLowEnergy, adapter()->state().type());
  EXPECT_FALSE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, InitializeSuccess) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // Return valid buffer information and enable LE support. (This should
  // succeed).
  FakeController::Settings settings;
  settings.lmp_features_page0 |=
      static_cast<uint64_t>(hci::LMPFeature::kLESupported);
  settings.le_acl_data_packet_length = 5;
  settings.le_total_num_acl_data_packets = 1;
  test_device()->set_settings(settings);

  InitializeAdapter(std::move(init_cb));
  EXPECT_TRUE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_TRUE(adapter()->state().IsLowEnergySupported());
  EXPECT_TRUE(adapter()->state().IsBREDRSupported());
  EXPECT_EQ(TechnologyType::kDualMode, adapter()->state().type());
  EXPECT_FALSE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, InitializeFailureHCICommandError) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // Make all settings valid but make an HCI command fail.
  FakeController::Settings settings;
  settings.ApplyLEOnlyDefaults();
  test_device()->set_settings(settings);
  test_device()->SetDefaultResponseStatus(hci::kLEReadLocalSupportedFeatures,
                                          hci::StatusCode::kHardwareFailure);

  InitializeAdapter(std::move(init_cb));
  EXPECT_FALSE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_FALSE(adapter()->state().IsLowEnergySupported());
  EXPECT_FALSE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, TransportClosedCallback) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  FakeController::Settings settings;
  settings.ApplyLEOnlyDefaults();
  test_device()->set_settings(settings);

  InitializeAdapter(std::move(init_cb));
  EXPECT_TRUE(success);
  EXPECT_EQ(1, init_cb_count);
  EXPECT_TRUE(adapter()->state().IsLowEnergySupported());
  EXPECT_FALSE(transport_closed_called());

  // Deleting the FakeController should cause the transport closed callback to
  // get called.
  async::PostTask(dispatcher(), [this] { DeleteTestDevice(); });
  RunLoopUntilIdle();

  EXPECT_TRUE(transport_closed_called());
}

TEST_F(GAP_AdapterTest, SetNameError) {
  std::string kNewName = "something";
  bool success;
  hci::Status result;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  // Make all settings valid but make WriteLocalName command fail.
  FakeController::Settings settings;
  settings.ApplyDualModeDefaults();
  test_device()->set_settings(settings);
  test_device()->SetDefaultResponseStatus(hci::kWriteLocalName,
                                          hci::StatusCode::kHardwareFailure);

  InitializeAdapter(std::move(init_cb));
  EXPECT_TRUE(success);
  EXPECT_EQ(1, init_cb_count);

  auto name_cb = [&result](const auto& status) { result = status; };

  adapter()->SetLocalName(kNewName, name_cb);

  RunLoopUntilIdle();

  EXPECT_FALSE(result);
  EXPECT_EQ(hci::StatusCode::kHardwareFailure, result.protocol_error());
}

TEST_F(GAP_AdapterTest, SetNameSuccess) {
  const std::string kNewName = "Fuchsia BT 💖✨";
  bool success;
  hci::Status result;
  int init_cb_count = 0;
  auto init_cb = [&](bool cb_success) {
    success = cb_success;
    init_cb_count++;
  };

  FakeController::Settings settings;
  settings.ApplyDualModeDefaults();
  test_device()->set_settings(settings);

  InitializeAdapter(std::move(init_cb));
  EXPECT_TRUE(success);
  EXPECT_EQ(1, init_cb_count);

  auto name_cb = [&result](const auto& status) { result = status; };

  adapter()->SetLocalName(kNewName, name_cb);

  RunLoopUntilIdle();

  EXPECT_TRUE(result);
  // Local name is only valid up to the first zero
  for (size_t i = 0; i < kNewName.size(); i++) {
    EXPECT_EQ(kNewName[i], test_device()->local_name()[i]);
  }
}

TEST_F(GAP_AdapterTest, PeerCacheReturnsNonNull) {
  EXPECT_TRUE(adapter()->peer_cache());
}

TEST_F(GAP_AdapterTest, LeAutoConnect) {
  constexpr zx::duration kTestScanPeriod = zx::sec(10);
  constexpr PeerId kPeerId(1234);

  FakeController::Settings settings;
  settings.ApplyLEOnlyDefaults();
  test_device()->set_settings(settings);

  InitializeAdapter([](bool) {});
  adapter()->le_discovery_manager()->set_scan_period(kTestScanPeriod);

  auto fake_peer = std::make_unique<FakePeer>(kTestAddr, true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  LowEnergyConnectionRefPtr conn;
  adapter()->set_auto_connect_callback(
      [&](auto conn_ref) { conn = std::move(conn_ref); });

  // Enable background scanning. No auto-connect should take place since the
  // device isn't yet bonded.
  adapter()->le_discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(conn);
  EXPECT_EQ(0u, adapter()->peer_cache()->count());

  // Mark the peer as bonded and advance the scan period.
  sm::PairingData pdata;
  pdata.ltk = sm::LTK();
  adapter()->peer_cache()->AddBondedPeer(kPeerId, kTestAddr, pdata, {});
  EXPECT_EQ(1u, adapter()->peer_cache()->count());
  RunLoopFor(kTestScanPeriod);

  // The peer should have been auto-connected.
  ASSERT_TRUE(conn);
  EXPECT_EQ(kPeerId, conn->peer_identifier());
}

// Tests the interactions between the advertising manager and the local address
// manager when the controller uses legacy advertising.
TEST_F(GAP_AdapterTest, LocalAddressForLegacyAdvertising) {
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  test_device()->set_settings(settings);
  InitializeAdapter([](bool) {});

  AdvertisementId adv_id = kInvalidAdvertisementId;
  auto adv_cb = [&](auto id, hci::Status status) {
    adv_id = id;
    EXPECT_TRUE(status);
  };

  // Advertising should use the public address by default.
  AdvertisingData empty_data;
  adapter()->le_advertising_manager()->StartAdvertising(
      empty_data, empty_data, nullptr, zx::msec(60), false, adv_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_advertising_state().own_address_type);

  // Enable privacy. The random address should not get configured while
  // advertising is in progress.
  adapter()->le_address_manager()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_random_address());

  // Stop advertising.
  adapter()->le_advertising_manager()->StopAdvertising(adv_id);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  EXPECT_FALSE(test_device()->le_random_address());

  // Restart advertising. This should configure the LE random address and
  // advertise using it.
  adapter()->le_advertising_manager()->StartAdvertising(
      empty_data, empty_data, nullptr, zx::msec(60), false, adv_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_random_address());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_advertising_state().own_address_type);

  // Advance time to force the random address to refresh. The update should be
  // deferred while advertising.
  auto last_random_addr = *test_device()->le_random_address();
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(last_random_addr, *test_device()->le_random_address());

  // Restarting advertising should refresh the controller address.
  adapter()->le_advertising_manager()->StopAdvertising(adv_id);
  adapter()->le_advertising_manager()->StartAdvertising(
      empty_data, empty_data, nullptr, zx::msec(60), false, adv_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_advertising_state().own_address_type);
  EXPECT_TRUE(test_device()->le_random_address());
  EXPECT_NE(last_random_addr, test_device()->le_random_address());

  // Disable privacy. The next time advertising gets started it should use a
  // public address.
  adapter()->le_address_manager()->EnablePrivacy(false);
  adapter()->le_advertising_manager()->StopAdvertising(adv_id);
  adapter()->le_advertising_manager()->StartAdvertising(
      empty_data, empty_data, nullptr, zx::msec(60), false, adv_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_advertising_state().own_address_type);
}

// Tests the interactions between the discovery manager and the local address
// manager.
TEST_F(GAP_AdapterTest, LocalAddressForDiscovery) {
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  test_device()->set_settings(settings);
  InitializeAdapter([](bool) {});

  // Set a scan period that is longer than the private address timeout, for
  // testing.
  constexpr auto kTestDelay = zx::sec(5);
  constexpr auto kTestScanPeriod = kPrivateAddressTimeout + kTestDelay;
  adapter()->le_discovery_manager()->set_scan_period(kTestScanPeriod);

  // Discovery should use the public address by default.
  LowEnergyDiscoverySessionPtr session;
  auto cb = [&](auto s) { session = std::move(s); };
  adapter()->le_discovery_manager()->StartDiscovery(cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(session);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_scan_state().own_address_type);

  // Enable privacy. The random address should not get configured while a scan
  // is in progress.
  adapter()->le_address_manager()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_random_address());

  // Stop discovery.
  session = nullptr;
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_FALSE(test_device()->le_random_address());

  // Restart discovery. This should configure the LE random address and scan
  // using it.
  adapter()->le_discovery_manager()->StartDiscovery(cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(session);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_scan_state().own_address_type);

  // Advance time to force the random address to refresh. The update should be
  // deferred while still scanning.
  ASSERT_TRUE(test_device()->le_random_address());
  auto last_random_addr = *test_device()->le_random_address();
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(last_random_addr, *test_device()->le_random_address());

  // Let the scan period expire. This should restart scanning and refresh the
  // random address.
  RunLoopFor(kTestDelay);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_scan_state().own_address_type);
  ASSERT_TRUE(test_device()->le_random_address());
  EXPECT_NE(last_random_addr, test_device()->le_random_address());

  // Disable privacy. The next time scanning gets started it should use a
  // public address.
  adapter()->le_address_manager()->EnablePrivacy(false);
  RunLoopFor(kTestScanPeriod);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_scan_state().own_address_type);
}

TEST_F(GAP_AdapterTest, LocalAddressForConnections) {
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  test_device()->set_settings(settings);
  InitializeAdapter([](bool) {});

  // Set-up a device for testing.
  auto* peer = adapter()->peer_cache()->NewPeer(kTestAddr, true);
  auto fake_peer = std::make_unique<FakePeer>(kTestAddr);
  test_device()->AddPeer(std::move(fake_peer));

  LowEnergyConnectionRefPtr conn_ref;
  auto connect_cb = [&](auto status, auto c) {
    ASSERT_TRUE(status);
    conn_ref = std::move(c);
  };

  // A connection request should use the public address by default.
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);

  // Enable privacy. The random address should not get configured while a
  // connection attempt is in progress.
  adapter()->le_address_manager()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_random_address());
  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(test_device()->le_connect_params());
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_connect_params()->own_address_type);

  // Create a new connection. The second attempt should use a random address.
  conn_ref = nullptr;
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_random_address());
  ASSERT_TRUE(conn_ref);
  ASSERT_TRUE(test_device()->le_connect_params());
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_connect_params()->own_address_type);

  // Disable privacy. The next connection attempt should use a public address.
  adapter()->le_address_manager()->EnablePrivacy(false);
  conn_ref = nullptr;
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_connect_params()->own_address_type);
}

// Tests the deferral of random address configuration while a connection request
// is outstanding.
TEST_F(GAP_AdapterTest, LocalAddressDuringHangingConnect) {
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  test_device()->set_settings(settings);
  InitializeAdapter([](bool) {});

  // Add a device to the cache but not the fake controller. This will cause the
  // connection request to hang.
  auto* peer = adapter()->peer_cache()->NewPeer(kTestAddr, true);

  constexpr auto kTestDelay = zx::sec(5);
  constexpr auto kTestTimeout = kPrivateAddressTimeout + kTestDelay;

  // Some of the behavior below stems from the fact that kTestTimeout is longer
  // than kCacheTimeout. This assertion is here to catch regressions in this
  // test if the values ever change.
  // TODO(BT-825): Configuring the cache expiration timeout explicitly would
  // remove some of the unnecessary invariants from this test case.
  static_assert(kTestTimeout > kCacheTimeout,
                "expected a shorter device cache timeout");

  adapter()->le_connection_manager()->set_request_timeout_for_testing(
      kTestTimeout);

  // The connection request should use a public address.
  hci::Status status;
  auto connect_cb = [&](auto s, auto conn_ref) {
    status = s;
    ASSERT_FALSE(conn_ref);
  };
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_connect_params());
  EXPECT_EQ(hci::LEOwnAddressType::kPublic,
            test_device()->le_connect_params()->own_address_type);

  // Enable privacy. The random address should not get configured while a
  // connection request is outstanding.
  adapter()->le_address_manager()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_random_address());

  // Let the connection request timeout.
  RunLoopFor(kTestTimeout);
  EXPECT_EQ(HostError::kTimedOut, status.error());

  // The peer should not have expired.
  ASSERT_EQ(peer, adapter()->peer_cache()->FindByAddress(kTestAddr));
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(test_device()->le_random_address());
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_connect_params()->own_address_type);

  // Advance the time to cause the random address to refresh. The update should
  // be deferred while a connection request is outstanding.
  auto last_random_addr = *test_device()->le_random_address();
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(last_random_addr, *test_device()->le_random_address());

  ASSERT_EQ(peer, adapter()->peer_cache()->FindByAddress(kTestAddr));

  // The address should refresh after the next connection attempt.
  RunLoopFor(kTestDelay);
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  EXPECT_NE(last_random_addr, *test_device()->le_random_address());
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_connect_params()->own_address_type);
}

// Tests that existing connections don't prevent an address change.
TEST_F(GAP_AdapterTest, ExistingConnectionDoesNotPreventLocalAddressChange) {
  FakeController::Settings settings;
  settings.ApplyLegacyLEConfig();
  test_device()->set_settings(settings);
  InitializeAdapter([](bool) {});

  adapter()->le_address_manager()->EnablePrivacy(true);

  LowEnergyConnectionRefPtr conn_ref;
  auto connect_cb = [&](auto status, auto c) {
    ASSERT_TRUE(status);
    ASSERT_TRUE(c);
    conn_ref = std::move(c);
  };

  auto* peer = adapter()->peer_cache()->NewPeer(kTestAddr, true);
  auto fake_peer = std::make_unique<FakePeer>(kTestAddr);
  test_device()->AddPeer(std::move(fake_peer));
  adapter()->le_connection_manager()->Connect(peer->identifier(), connect_cb);
  RunLoopUntilIdle();
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_connect_params()->own_address_type);

  // Expire the private address. The address should refresh without interference
  // from the ongoing connection.
  ASSERT_TRUE(test_device()->le_random_address());
  auto last_random_addr = *test_device()->le_random_address();
  RunLoopFor(kPrivateAddressTimeout);
  ASSERT_TRUE(test_device()->le_random_address());
  EXPECT_NE(last_random_addr, *test_device()->le_random_address());
}

}  // namespace
}  // namespace gap
}  // namespace bt
