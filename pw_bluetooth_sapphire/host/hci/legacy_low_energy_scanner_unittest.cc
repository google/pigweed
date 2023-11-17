// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_scanner.h"

#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci/fake_local_address_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_peer.h"

namespace bt::hci {
namespace {

using bt::testing::FakeController;
using bt::testing::FakePeer;
using TestingBase = bt::testing::FakeDispatcherControllerTest<FakeController>;

constexpr pw::chrono::SystemClock::duration kScanPeriod =
    std::chrono::seconds(10);
constexpr pw::chrono::SystemClock::duration kPwScanPeriod =
    std::chrono::seconds(10);
constexpr pw::chrono::SystemClock::duration kScanResponseTimeout =
    std::chrono::seconds(2);
constexpr pw::chrono::SystemClock::duration kPwScanResponseTimeout =
    std::chrono::seconds(2);

// The unit tests below assume that the scan period is longer than the scan
// response timeout when exercising timeout expiration.
static_assert(kScanResponseTimeout < kScanPeriod,
              "expected a smaller scan response timeout for testing");

const StaticByteBuffer kPlainAdvDataBytes('T', 'e', 's', 't');
const StaticByteBuffer kPlainScanRspBytes('D', 'a', 't', 'a');

constexpr char kPlainAdvData[] = "Test";
constexpr char kPlainScanRsp[] = "Data";
constexpr char kAdvDataAndScanRsp[] = "TestData";

const DeviceAddress kPublicAddress1(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kPublicAddress2(DeviceAddress::Type::kLEPublic, {2});

const DeviceAddress kRandomAddress1(DeviceAddress::Type::kLERandom, {3});
const DeviceAddress kRandomAddress2(DeviceAddress::Type::kLERandom, {4});
const DeviceAddress kRandomAddress3(DeviceAddress::Type::kLERandom, {5});
const DeviceAddress kRandomAddress4(DeviceAddress::Type::kLERandom, {6});

class LegacyLowEnergyScannerTest : public TestingBase,
                                   public LowEnergyScanner::Delegate {
 public:
  LegacyLowEnergyScannerTest() = default;
  ~LegacyLowEnergyScannerTest() override = default;

 protected:
  // TestingBase overrides:
  void SetUp() override {
    TestingBase::SetUp();

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    scanner_ = std::make_unique<LegacyLowEnergyScanner>(
        &fake_address_delegate_, transport()->GetWeakPtr(), dispatcher());
    scanner_->set_delegate(this);
  }

  void TearDown() override {
    scanner_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  using PeerFoundCallback =
      fit::function<void(const LowEnergyScanResult&, const ByteBuffer&)>;
  void set_peer_found_callback(PeerFoundCallback cb) {
    peer_found_cb_ = std::move(cb);
  }

  using DirectedAdvCallback = fit::function<void(const LowEnergyScanResult&)>;
  void set_directed_adv_callback(DirectedAdvCallback cb) {
    directed_adv_cb_ = std::move(cb);
  }

  bool StartScan(bool active,
                 pw::chrono::SystemClock::duration period =
                     LowEnergyScanner::kPeriodInfinite) {
    LowEnergyScanner::ScanOptions options{
        .active = active,
        .filter_duplicates = true,
        .period = period,
        .scan_response_timeout = kPwScanResponseTimeout};
    return scanner()->StartScan(
        options, [this](auto status) { last_scan_status_ = status; });
  }

  // LowEnergyScanner::Observer override:
  void OnPeerFound(const LowEnergyScanResult& result,
                   const ByteBuffer& data) override {
    if (peer_found_cb_) {
      peer_found_cb_(result, data);
    }
  }

  // LowEnergyScanner::Observer override:
  void OnDirectedAdvertisement(const LowEnergyScanResult& result) override {
    if (directed_adv_cb_) {
      directed_adv_cb_(result);
    }
  }

  // Adds 6 fake peers using kAddress[0-5] above.
  void AddFakePeers() {
    // Generates ADV_IND, scan response is reported in a single HCI event.
    auto fake_peer =
        std::make_unique<FakePeer>(kPublicAddress1, dispatcher(), true, true);
    fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
    fake_peer->SetScanResponse(/*should_batch_reports=*/true,
                               kPlainScanRspBytes);
    test_device()->AddPeer(std::move(fake_peer));

    // Generates ADV_SCAN_IND, scan response is reported over multiple HCI
    // events.
    fake_peer =
        std::make_unique<FakePeer>(kRandomAddress1, dispatcher(), false, true);
    fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
    fake_peer->SetScanResponse(/*should_batch_reports=*/false,
                               kPlainScanRspBytes);
    test_device()->AddPeer(std::move(fake_peer));

    // Generates ADV_IND, empty scan response is reported over multiple HCI
    // events.
    fake_peer =
        std::make_unique<FakePeer>(kPublicAddress2, dispatcher(), true, true);
    fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
    fake_peer->SetScanResponse(/*should_batch_reports=*/false,
                               DynamicByteBuffer());
    test_device()->AddPeer(std::move(fake_peer));

    // Generates ADV_IND, empty adv data and non-empty scan response is reported
    // over multiple HCI events.
    fake_peer =
        std::make_unique<FakePeer>(kRandomAddress2, dispatcher(), true, true);
    fake_peer->SetScanResponse(/*should_batch_reports=*/false,
                               kPlainScanRspBytes);
    test_device()->AddPeer(std::move(fake_peer));

    // Generates ADV_IND, a scan response is never sent even though ADV_IND is
    // scannable.
    fake_peer =
        std::make_unique<FakePeer>(kRandomAddress3, dispatcher(), true, false);
    fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
    test_device()->AddPeer(std::move(fake_peer));

    // Generates ADV_NONCONN_IND
    fake_peer =
        std::make_unique<FakePeer>(kRandomAddress4, dispatcher(), false, false);
    fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
    test_device()->AddPeer(std::move(fake_peer));
  }

  LegacyLowEnergyScanner* scanner() const { return scanner_.get(); }
  FakeLocalAddressDelegate* fake_address_delegate() {
    return &fake_address_delegate_;
  }

  LowEnergyScanner::ScanStatus last_scan_status() const {
    return last_scan_status_;
  }

 private:
  PeerFoundCallback peer_found_cb_;
  DirectedAdvCallback directed_adv_cb_;
  FakeLocalAddressDelegate fake_address_delegate_{dispatcher()};
  std::unique_ptr<LegacyLowEnergyScanner> scanner_;

  LowEnergyScanner::ScanStatus last_scan_status_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyScannerTest);
};

using HCI_LegacyLowEnergyScannerTest = LegacyLowEnergyScannerTest;

TEST_F(LegacyLowEnergyScannerTest, StartScanHCIErrors) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  // Set Scan Parameters will fail.
  test_device()->SetDefaultResponseStatus(
      hci_spec::kLESetScanParameters,
      pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE);
  EXPECT_EQ(0, test_device()->le_scan_state().scan_interval);

  EXPECT_TRUE(StartScan(false));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  // Calling StartScan() should fail as the state is not kIdle.
  EXPECT_FALSE(StartScan(false));
  RunUntilIdle();

  // Status should be failure and the scan parameters shouldn't have applied.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kFailed, last_scan_status());
  EXPECT_EQ(0, test_device()->le_scan_state().scan_interval);
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());

  // Set Scan Parameters will succeed but Set Scan Enable will fail.
  test_device()->ClearDefaultResponseStatus(hci_spec::kLESetScanParameters);
  test_device()->SetDefaultResponseStatus(
      hci_spec::kLESetScanEnable,
      pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE);

  EXPECT_TRUE(StartScan(false));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunUntilIdle();

  // Status should be failure but the scan parameters should have applied.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kFailed, last_scan_status());
  EXPECT_EQ(hci_spec::defaults::kLEScanInterval,
            test_device()->le_scan_state().scan_interval);
  EXPECT_EQ(hci_spec::defaults::kLEScanWindow,
            test_device()->le_scan_state().scan_window);
  EXPECT_EQ(pw::bluetooth::emboss::LEScanFilterPolicy::BASIC_UNFILTERED,
            test_device()->le_scan_state().filter_policy);
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(LegacyLowEnergyScannerTest, StartScan) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  EXPECT_TRUE(StartScan(true, kPwScanPeriod));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunUntilIdle();

  // Scan should have started.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kActive, last_scan_status());
  EXPECT_EQ(hci_spec::defaults::kLEScanInterval,
            test_device()->le_scan_state().scan_interval);
  EXPECT_EQ(hci_spec::defaults::kLEScanWindow,
            test_device()->le_scan_state().scan_window);
  EXPECT_EQ(pw::bluetooth::emboss::LEScanFilterPolicy::BASIC_UNFILTERED,
            test_device()->le_scan_state().filter_policy);
  EXPECT_EQ(pw::bluetooth::emboss::LEScanType::ACTIVE,
            test_device()->le_scan_state().scan_type);
  EXPECT_TRUE(test_device()->le_scan_state().filter_duplicates);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());
  EXPECT_TRUE(scanner()->IsScanning());

  // Calling StartScan should fail as a scan is already in progress.
  EXPECT_FALSE(StartScan(true));

  // After 10 s (kScanPeriod) the scan should stop by itself.
  RunFor(kScanPeriod);

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kComplete, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(LegacyLowEnergyScannerTest, StopScan) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  // Calling StopScan should fail while a scan is not in progress.
  EXPECT_FALSE(scanner()->StopScan());

  // Pass a long scan period value. This should not matter as we will terminate
  // the scan directly.
  EXPECT_TRUE(StartScan(true, kPwScanPeriod * 10u));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunUntilIdle();

  // Scan should have started.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kActive, last_scan_status());
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());
  EXPECT_TRUE(scanner()->IsScanning());

  // StopScan() should terminate the scan session and the status should be
  // kStopped.
  EXPECT_TRUE(scanner()->StopScan());
  RunUntilIdle();

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kStopped, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(LegacyLowEnergyScannerTest, StopScanWhileInitiating) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  // Call StopScan(). This should cancel the HCI command sequence set up by
  // StartScan() so that the it never completes. The HCI_LE_Set_Scan_Parameters
  // command *may* get sent but the scan should never get enabled.
  EXPECT_TRUE(scanner()->StopScan());
  RunUntilIdle();

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kStopped, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(LegacyLowEnergyScannerTest, ScanResponseTimeout) {
  constexpr pw::chrono::SystemClock::duration kHalfTimeout =
      kScanResponseTimeout / 2;

  std::unordered_set<DeviceAddress> results;
  set_peer_found_callback([&](const auto& result, const auto& data) {
    results.insert(result.address);
  });

  // Add a peer that sends a scan response and one that doesn't.
  auto fake_peer =
      std::make_unique<FakePeer>(kRandomAddress1, dispatcher(), false, true);
  fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
  fake_peer->SetScanResponse(/*should_batch_reports=*/false,
                             kPlainScanRspBytes);
  test_device()->AddPeer(std::move(fake_peer));

  fake_peer =
      std::make_unique<FakePeer>(kRandomAddress2, dispatcher(), true, false);
  fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
  test_device()->AddPeer(std::move(fake_peer));

  EXPECT_TRUE(StartScan(true));
  RunUntilIdle();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(1u, results.count(kRandomAddress1));

  // Advance the time but do not expire the timeout.
  RunFor(kHalfTimeout);
  ASSERT_EQ(1u, results.size());

  // Add another peer that doesn't send a scan response after the kHalfTimeout
  // delay. This is to test that a separate timeout is kept for every peer.
  fake_peer =
      std::make_unique<FakePeer>(kRandomAddress3, dispatcher(), true, false);
  fake_peer->SetAdvertisingData(kPlainAdvDataBytes);
  test_device()->AddPeer(std::move(fake_peer));

  // Expire the first timeout.
  RunFor(kHalfTimeout);
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(1u, results.count(kRandomAddress1));
  EXPECT_EQ(1u, results.count(kRandomAddress2));

  // Expire the second timeout.
  RunFor(kHalfTimeout);
  ASSERT_EQ(3u, results.size());
  EXPECT_EQ(1u, results.count(kRandomAddress1));
  EXPECT_EQ(1u, results.count(kRandomAddress2));
  EXPECT_EQ(1u, results.count(kRandomAddress3));
}

TEST_F(LegacyLowEnergyScannerTest, ActiveScanResults) {
  // One of the 6 fake peers is scannable but never sends scan response
  // packets. That peer doesn't get reported until the end of the scan period.
  constexpr size_t kExpectedResultCount = 5u;

  AddFakePeers();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_peer_found_callback([&](const auto& result, const auto& data) {
    results[result.address] = std::make_pair(result, data.ToString());
  });

  // Perform an active scan.
  EXPECT_TRUE(StartScan(true, kPwScanPeriod));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunUntilIdle();

  ASSERT_EQ(kExpectedResultCount, results.size());

  // Ending the scan period should notify Fake Peer #4.
  RunFor(kScanPeriod);
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kComplete, last_scan_status());
  ASSERT_EQ(kExpectedResultCount + 1, results.size());

  // Verify the 6 results against the fake peers that were set up by
  // AddFakePeers(). Since the scan period ended naturally, LowEnergyScanner
  // should generate a peer found event for all pending reports even if a scan
  // response was not received for a scannable peer (see Fake Peer 4, i.e.
  // kRandomAddress3).

  // Result 0 (ADV_IND)
  {
    const auto& iter = results.find(kPublicAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kAdvDataAndScanRsp, result_pair.second);
    EXPECT_EQ(kPublicAddress1, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 1 (ADV_SCAN_IND)
  {
    const auto& iter = results.find(kRandomAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kAdvDataAndScanRsp, result_pair.second);
    EXPECT_EQ(kRandomAddress1, result_pair.first.address);
    EXPECT_FALSE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 2 (ADV_IND), empty scan response
  {
    const auto& iter = results.find(kPublicAddress2);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kPublicAddress2, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 3 (ADV_IND), empty advertising data w/ scan response
  {
    const auto& iter = results.find(kRandomAddress2);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainScanRsp, result_pair.second);
    EXPECT_EQ(kRandomAddress2, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 4 (ADV_IND), no scan response
  {
    const auto& iter = results.find(kRandomAddress3);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kRandomAddress3, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 5 (ADV_NONCONN_IND)
  {
    const auto& iter = results.find(kRandomAddress4);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kRandomAddress4, result_pair.first.address);
    EXPECT_FALSE(result_pair.first.connectable);
    results.erase(iter);
  }

  // No other reports are expected
  EXPECT_TRUE(results.empty());
}

TEST_F(LegacyLowEnergyScannerTest, StopDuringActiveScan) {
  AddFakePeers();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_peer_found_callback([&results](const auto& result, const auto& data) {
    results[result.address] = std::make_pair(result, data.ToString());
  });

  // Perform an active scan indefinitely. This means that the scan period will
  // never complete by itself.
  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunUntilIdle();
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());

  // Run the loop until we've seen an event for the last peer that we
  // added. Fake Peer 4 (i.e. kRandomAddress3) is scannable but it never sends
  // a scan response so we expect that to remain in the scanner's pending
  // reports list.
  RunUntilIdle();
  EXPECT_EQ(5u, results.size());
  EXPECT_EQ(results.find(kRandomAddress3), results.end());

  // Stop the scan. Since we are terminating the scan period early,
  // LowEnergyScanner should not send a report for the pending peer.
  EXPECT_TRUE(scanner()->StopScan());
  RunUntilIdle();
  EXPECT_TRUE(scanner()->IsIdle());

  EXPECT_EQ(5u, results.size());
  EXPECT_EQ(results.find(kRandomAddress3), results.end());
}

TEST_F(LegacyLowEnergyScannerTest, PassiveScanResults) {
  constexpr size_t kExpectedResultCount = 6u;
  AddFakePeers();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_peer_found_callback([&](const auto& result, const auto& data) {
    results[result.address] = std::make_pair(result, data.ToString());
  });

  // Perform a passive scan.
  EXPECT_TRUE(StartScan(false));

  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunUntilIdle();
  EXPECT_EQ(LowEnergyScanner::State::kPassiveScanning, scanner()->state());
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kPassive, last_scan_status());
  ASSERT_EQ(kExpectedResultCount, results.size());

  // Verify the 6 results against the fake peers that were set up by
  // AddFakePeers(). All Scan Response PDUs should have been ignored.

  // Result 0
  {
    const auto& iter = results.find(kPublicAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kPublicAddress1, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 1
  {
    const auto& iter = results.find(kRandomAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kRandomAddress1, result_pair.first.address);
    EXPECT_FALSE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 2
  {
    const auto& iter = results.find(kPublicAddress2);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kPublicAddress2, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 3
  {
    const auto& iter = results.find(kRandomAddress2);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ("", result_pair.second);
    EXPECT_EQ(kRandomAddress2, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 4
  {
    const auto& iter = results.find(kRandomAddress3);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kRandomAddress3, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 5
  {
    const auto& iter = results.find(kRandomAddress4);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kPlainAdvData, result_pair.second);
    EXPECT_EQ(kRandomAddress4, result_pair.first.address);
    EXPECT_FALSE(result_pair.first.connectable);
    results.erase(iter);
  }

  EXPECT_TRUE(results.empty());
}

TEST_F(LegacyLowEnergyScannerTest, DirectedReport) {
  const auto& kPublicUnresolved = kPublicAddress1;
  const auto& kPublicResolved = kPublicAddress2;
  const auto& kRandomUnresolved = kRandomAddress1;
  const auto& kRandomResolved = kRandomAddress2;
  constexpr size_t kExpectedResultCount = 4u;

  // Unresolved public.
  auto fake_peer =
      std::make_unique<FakePeer>(kPublicUnresolved, dispatcher(), true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  // Unresolved random.
  fake_peer =
      std::make_unique<FakePeer>(kRandomUnresolved, dispatcher(), true, false);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  // Resolved public.
  fake_peer =
      std::make_unique<FakePeer>(kPublicResolved, dispatcher(), true, false);
  fake_peer->set_address_resolved(true);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  // Resolved random.
  fake_peer =
      std::make_unique<FakePeer>(kRandomResolved, dispatcher(), true, false);
  fake_peer->set_address_resolved(true);
  fake_peer->enable_directed_advertising(true);
  test_device()->AddPeer(std::move(fake_peer));

  std::unordered_map<DeviceAddress, LowEnergyScanResult> results;
  set_directed_adv_callback(
      [&](const auto& result) { results[result.address] = result; });

  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunUntilIdle();

  ASSERT_EQ(LowEnergyScanner::ScanStatus::kActive, last_scan_status());
  ASSERT_EQ(kExpectedResultCount, results.size());

  ASSERT_TRUE(results.count(kPublicUnresolved));
  EXPECT_FALSE(results[kPublicUnresolved].resolved);

  ASSERT_TRUE(results.count(kRandomUnresolved));
  EXPECT_FALSE(results[kRandomUnresolved].resolved);

  ASSERT_TRUE(results.count(kPublicResolved));
  EXPECT_TRUE(results[kPublicResolved].resolved);

  ASSERT_TRUE(results.count(kRandomResolved));
  EXPECT_TRUE(results[kRandomResolved].resolved);
}

TEST_F(LegacyLowEnergyScannerTest, AllowsRandomAddressChange) {
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());
  EXPECT_TRUE(StartScan(false));

  // Address change should not be allowed while the procedure is pending.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());

  RunUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());
}

TEST_F(LegacyLowEnergyScannerTest,
       AllowsRandomAddressChangeWhileRequestingLocalAddress) {
  // Make the local address delegate report its result asynchronously.
  fake_address_delegate()->set_async(true);
  EXPECT_TRUE(StartScan(false));

  // The scanner should be in the initiating state without initiating controller
  // procedures that would prevent a local address change.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());

  RunUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());
}

TEST_F(LegacyLowEnergyScannerTest, ScanUsingPublicAddress) {
  fake_address_delegate()->set_local_address(kPublicAddress1);
  EXPECT_TRUE(StartScan(false));
  RunUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_EQ(pw::bluetooth::emboss::LEOwnAddressType::PUBLIC,
            test_device()->le_scan_state().own_address_type);
}

TEST_F(LegacyLowEnergyScannerTest, ScanUsingRandomAddress) {
  fake_address_delegate()->set_local_address(kRandomAddress1);
  EXPECT_TRUE(StartScan(false));
  RunUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_EQ(pw::bluetooth::emboss::LEOwnAddressType::RANDOM,
            test_device()->le_scan_state().own_address_type);
}

TEST_F(LegacyLowEnergyScannerTest, StopScanWhileWaitingForLocalAddress) {
  fake_address_delegate()->set_async(true);
  EXPECT_TRUE(StartScan(false));

  // Should be waiting for the random address.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());

  EXPECT_TRUE(scanner()->StopScan());
  RunUntilIdle();

  // Should end up not scanning.
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
}

}  // namespace
}  // namespace bt::hci
