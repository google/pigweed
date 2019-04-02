// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_device.h"
#include "src/lib/fxl/macros.h"

namespace bt {
namespace hci {
namespace {

using common::DeviceAddress;
using testing::FakeController;
using testing::FakeDevice;
using TestingBase = testing::FakeControllerTest<FakeController>;

constexpr zx::duration kScanPeriod = zx::sec(10);

constexpr char kPlainAdvData[] = "Test";
constexpr char kPlainScanRsp[] = "Data";
constexpr char kAdvDataAndScanRsp[] = "TestData";

const DeviceAddress kPublicAddress1(DeviceAddress::Type::kLEPublic,
                                    "00:00:00:00:00:01");
const DeviceAddress kPublicAddress2(DeviceAddress::Type::kLEPublic,
                                    "00:00:00:00:00:02");

const DeviceAddress kRandomAddress1(DeviceAddress::Type::kLERandom,
                                    "00:00:00:00:00:01");
const DeviceAddress kRandomAddress2(DeviceAddress::Type::kLERandom,
                                    "00:00:00:00:00:02");
const DeviceAddress kRandomAddress3(DeviceAddress::Type::kLERandom,
                                    "00:00:00:00:00:03");
const DeviceAddress kRandomAddress4(DeviceAddress::Type::kLERandom,
                                    "00:00:00:00:00:04");

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
        &fake_address_delegate_, transport(), dispatcher());
    scanner_->set_delegate(this);

    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    scanner_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  using DeviceFoundCallback = fit::function<void(const LowEnergyScanResult&,
                                                 const common::ByteBuffer&)>;
  void set_device_found_callback(DeviceFoundCallback cb) {
    device_found_cb_ = std::move(cb);
  }

  using DirectedAdvCallback = fit::function<void(const LowEnergyScanResult&)>;
  void set_directed_adv_callback(DirectedAdvCallback cb) {
    directed_adv_cb_ = std::move(cb);
  }

  bool StartScan(bool active,
                 zx::duration period = LowEnergyScanner::kPeriodInfinite) {
    return scanner()->StartScan(
        active, defaults::kLEScanInterval, defaults::kLEScanWindow, true,
        LEScanFilterPolicy::kNoWhiteList, period,
        [this](auto status) { last_scan_status_ = status; });
  }

  // LowEnergyScanner::Observer override:
  void OnDeviceFound(const LowEnergyScanResult& result,
                     const common::ByteBuffer& data) override {
    if (device_found_cb_) {
      device_found_cb_(result, data);
    }
  }

  // LowEnergyScanner::Observer override:
  void OnDirectedAdvertisement(const LowEnergyScanResult& result) override {
    if (directed_adv_cb_) {
      directed_adv_cb_(result);
    }
  }

  // Adds 6 fake devices using kAddress[0-5] above.
  void AddFakeDevices() {
    // We use malformed data for testing purposes, as we don't care about
    // integrity here.
    auto adv_data = common::CreateStaticByteBuffer('T', 'e', 's', 't');
    auto scan_rsp = common::CreateStaticByteBuffer('D', 'a', 't', 'a');
    auto empty_data = common::DynamicByteBuffer();

    // Generates ADV_IND, scan response is reported in a single HCI event.
    auto fake_device =
        std::make_unique<FakeDevice>(kPublicAddress1, true, true);
    fake_device->SetAdvertisingData(adv_data);
    fake_device->SetScanResponse(true, scan_rsp);
    test_device()->AddDevice(std::move(fake_device));

    // Generates ADV_SCAN_IND, scan response is reported over multiple HCI
    // events.
    fake_device = std::make_unique<FakeDevice>(kRandomAddress1, false, true);
    fake_device->SetAdvertisingData(adv_data);
    fake_device->SetScanResponse(false, scan_rsp);
    test_device()->AddDevice(std::move(fake_device));

    // Generates ADV_IND, empty scan response is reported over multiple HCI
    // events.
    fake_device = std::make_unique<FakeDevice>(kPublicAddress2, true, true);
    fake_device->SetAdvertisingData(adv_data);
    fake_device->SetScanResponse(false, empty_data);
    test_device()->AddDevice(std::move(fake_device));

    // Generates ADV_IND, empty adv data and non-empty scan response is reported
    // over multiple HCI events.
    fake_device = std::make_unique<FakeDevice>(kRandomAddress2, true, true);
    fake_device->SetScanResponse(false, scan_rsp);
    test_device()->AddDevice(std::move(fake_device));

    // Generates ADV_IND, a scan response is never sent even though ADV_IND is
    // scannable.
    fake_device = std::make_unique<FakeDevice>(kRandomAddress3, true, false);
    fake_device->SetAdvertisingData(adv_data);
    test_device()->AddDevice(std::move(fake_device));

    // Generates ADV_NONCONN_IND
    fake_device = std::make_unique<FakeDevice>(kRandomAddress4, false, false);
    fake_device->SetAdvertisingData(adv_data);
    test_device()->AddDevice(std::move(fake_device));
  }

  LegacyLowEnergyScanner* scanner() const { return scanner_.get(); }
  FakeLocalAddressDelegate* fake_address_delegate() {
    return &fake_address_delegate_;
  }

  LowEnergyScanner::ScanStatus last_scan_status() const {
    return last_scan_status_;
  }

 private:
  DeviceFoundCallback device_found_cb_;
  DirectedAdvCallback directed_adv_cb_;
  FakeLocalAddressDelegate fake_address_delegate_;
  std::unique_ptr<LegacyLowEnergyScanner> scanner_;

  LowEnergyScanner::ScanStatus last_scan_status_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LegacyLowEnergyScannerTest);
};

using HCI_LegacyLowEnergyScannerTest = LegacyLowEnergyScannerTest;

TEST_F(HCI_LegacyLowEnergyScannerTest, StartScanHCIErrors) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  // Set Scan Parameters will fail.
  test_device()->SetDefaultResponseStatus(kLESetScanParameters,
                                          StatusCode::kHardwareFailure);
  EXPECT_EQ(0, test_device()->le_scan_state().scan_interval);

  EXPECT_TRUE(StartScan(false));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  // Calling StartScan() should fail as the state is not kIdle.
  EXPECT_FALSE(StartScan(false));
  RunLoopUntilIdle();

  // Status should be failure and the scan parameters shouldn't have applied.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kFailed, last_scan_status());
  EXPECT_EQ(0, test_device()->le_scan_state().scan_interval);
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());

  // Set Scan Parameters will succeed but Set Scan Enable will fail.
  test_device()->ClearDefaultResponseStatus(kLESetScanParameters);
  test_device()->SetDefaultResponseStatus(kLESetScanEnable,
                                          StatusCode::kHardwareFailure);

  EXPECT_TRUE(StartScan(false));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunLoopUntilIdle();

  // Status should be failure but the scan parameters should have applied.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kFailed, last_scan_status());
  EXPECT_EQ(defaults::kLEScanInterval,
            test_device()->le_scan_state().scan_interval);
  EXPECT_EQ(defaults::kLEScanWindow,
            test_device()->le_scan_state().scan_window);
  EXPECT_EQ(LEScanFilterPolicy::kNoWhiteList,
            test_device()->le_scan_state().filter_policy);
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, StartScan) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  EXPECT_TRUE(StartScan(true, kScanPeriod));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunLoopUntilIdle();

  // Scan should have started.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kActive, last_scan_status());
  EXPECT_EQ(defaults::kLEScanInterval,
            test_device()->le_scan_state().scan_interval);
  EXPECT_EQ(defaults::kLEScanWindow,
            test_device()->le_scan_state().scan_window);
  EXPECT_EQ(LEScanFilterPolicy::kNoWhiteList,
            test_device()->le_scan_state().filter_policy);
  EXPECT_EQ(LEScanType::kActive, test_device()->le_scan_state().scan_type);
  EXPECT_TRUE(test_device()->le_scan_state().filter_duplicates);
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());
  EXPECT_TRUE(scanner()->IsScanning());

  // Calling StartScan should fail as a scan is already in progress.
  EXPECT_FALSE(StartScan(true));

  // After 10 s (kScanPeriod) the scan should stop by itself.
  RunLoopFor(kScanPeriod);

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kComplete, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, StopScan) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  // Calling StopScan should fail while a scan is not in progress.
  EXPECT_FALSE(scanner()->StopScan());

  // Pass a long scan period value. This should not matter as we will terminate
  // the scan directly.
  EXPECT_TRUE(StartScan(true, kScanPeriod * 10u));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunLoopUntilIdle();

  // Scan should have started.
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kActive, last_scan_status());
  EXPECT_TRUE(test_device()->le_scan_state().enabled);
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());
  EXPECT_TRUE(scanner()->IsScanning());

  // StopScan() should terminate the scan session and the status should be
  // kStopped.
  EXPECT_TRUE(scanner()->StopScan());
  RunLoopUntilIdle();

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kStopped, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, StopScanWhileInitiating) {
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);

  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  // Call StopScan(). This should cancel the HCI command sequence set up by
  // StartScan() so that the it never completes. The HCI_LE_Set_Scan_Parameters
  // command *may* get sent but the scan should never get enabled.
  EXPECT_TRUE(scanner()->StopScan());
  RunLoopUntilIdle();

  EXPECT_EQ(LowEnergyScanner::ScanStatus::kStopped, last_scan_status());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(scanner()->IsScanning());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, ActiveScanResults) {
  // One of the 6 fake devices is scannable but never sends scan response
  // packets. That device doesn't get reported until the end of the scan period.
  constexpr size_t kExpectedResultCount = 5u;

  AddFakeDevices();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_device_found_callback([&, this](const auto& result, const auto& data) {
    results[result.address] = std::make_pair(result, data.ToString());
  });

  // Perform an active scan.
  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunLoopUntilIdle();

  ASSERT_EQ(kExpectedResultCount, results.size());

  // Ending the scan period should notify Fake Device #4.
  scanner()->StopScanPeriodForTesting();
  RunLoopUntilIdle();
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kComplete, last_scan_status());
  ASSERT_EQ(kExpectedResultCount + 1, results.size());

  // Verify the 6 results against the fake devices that were set up by
  // AddFakeDevices(). Since the scan period ended naturally, LowEnergyScanner
  // should generate a device found event for all pending reports even if a scan
  // response was not received for a scannable device (see Fake Device 4, i.e.
  // kRandomAddress3).

  // Result 0
  {
    const auto& iter = results.find(kPublicAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kAdvDataAndScanRsp, result_pair.second);
    EXPECT_EQ(kPublicAddress1, result_pair.first.address);
    EXPECT_TRUE(result_pair.first.connectable);
    results.erase(iter);
  }

  // Result 1
  {
    const auto& iter = results.find(kRandomAddress1);
    ASSERT_NE(iter, results.end());

    const auto& result_pair = iter->second;
    EXPECT_EQ(kAdvDataAndScanRsp, result_pair.second);
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
    EXPECT_EQ(kPlainScanRsp, result_pair.second);
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

TEST_F(HCI_LegacyLowEnergyScannerTest, StopDuringActiveScan) {
  AddFakeDevices();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_device_found_callback(
      [&results, this](const auto& result, const auto& data) {
        results[result.address] = std::make_pair(result, data.ToString());
      });

  // Perform an active scan indefinitely. This means that the scan period will
  // never complete by itself.
  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());
  RunLoopUntilIdle();
  EXPECT_EQ(LowEnergyScanner::State::kActiveScanning, scanner()->state());

  // Run the loop until we've seen an event for the last device that we
  // added. Fake Device 4 (i.e. kRandomAddress3) is scannable but it never sends
  // a scan response so we expect that remain in the scanner's pending reports
  // list.
  RunLoopUntilIdle();
  EXPECT_EQ(5u, results.size());
  EXPECT_EQ(results.find(kRandomAddress3), results.end());

  // Stop the scan. Since we are terminating the scan period early,
  // LowEnergyScanner should not send a report for the pending device.
  EXPECT_TRUE(scanner()->StopScan());
  RunLoopUntilIdle();
  EXPECT_TRUE(scanner()->IsIdle());

  EXPECT_EQ(5u, results.size());
  EXPECT_EQ(results.find(kRandomAddress3), results.end());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, PassiveScanResults) {
  constexpr size_t kExpectedResultCount = 6u;
  AddFakeDevices();

  std::map<DeviceAddress, std::pair<LowEnergyScanResult, std::string>> results;
  set_device_found_callback([&, this](const auto& result, const auto& data) {
    results[result.address] = std::make_pair(result, data.ToString());
  });

  // Perform a passive scan.
  EXPECT_TRUE(StartScan(false));

  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunLoopUntilIdle();
  EXPECT_EQ(LowEnergyScanner::State::kPassiveScanning, scanner()->state());
  EXPECT_EQ(LowEnergyScanner::ScanStatus::kPassive, last_scan_status());
  ASSERT_EQ(kExpectedResultCount, results.size());

  // Verify the 6 results against the fake devices that were set up by
  // AddFakeDevices(). All Scan Response PDUs should have been ignored.

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

TEST_F(HCI_LegacyLowEnergyScannerTest, DirectedReport) {
  const auto& kPublicUnresolved = kPublicAddress1;
  const auto& kPublicResolved = kPublicAddress2;
  const auto& kRandomUnresolved = kRandomAddress1;
  const auto& kRandomResolved = kRandomAddress2;
  constexpr size_t kExpectedResultCount = 4u;

  // Unresolved public.
  auto fake_dev = std::make_unique<FakeDevice>(kPublicUnresolved, true, false);
  fake_dev->enable_directed_advertising(true);
  test_device()->AddDevice(std::move(fake_dev));

  // Unresolved random.
  fake_dev = std::make_unique<FakeDevice>(kRandomUnresolved, true, false);
  fake_dev->enable_directed_advertising(true);
  test_device()->AddDevice(std::move(fake_dev));

  // Resolved public.
  fake_dev = std::make_unique<FakeDevice>(kPublicResolved, true, false);
  fake_dev->set_address_resolved(true);
  fake_dev->enable_directed_advertising(true);
  test_device()->AddDevice(std::move(fake_dev));

  // Resolved random.
  fake_dev = std::make_unique<FakeDevice>(kRandomResolved, true, false);
  fake_dev->set_address_resolved(true);
  fake_dev->enable_directed_advertising(true);
  test_device()->AddDevice(std::move(fake_dev));

  std::unordered_map<DeviceAddress, LowEnergyScanResult> results;
  set_directed_adv_callback(
      [&](const auto& result) { results[result.address] = result; });

  EXPECT_TRUE(StartScan(true));
  EXPECT_EQ(LowEnergyScanner::State::kInitiating, scanner()->state());

  RunLoopUntilIdle();

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

TEST_F(HCI_LegacyLowEnergyScannerTest, AllowsRandomAddressChange) {
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());
  EXPECT_TRUE(StartScan(false));

  // Address change should not be allowed while the procedure is pending.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());

  RunLoopUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());
}

TEST_F(HCI_LegacyLowEnergyScannerTest,
       AllowsRandomAddressChangeWhileRequestingLocalAddress) {
  // Make the local address delegate report its result asynchronously.
  fake_address_delegate()->set_async(true);
  EXPECT_TRUE(StartScan(false));

  // The scanner should be in the initiating state without initiating controller
  // procedures that would prevent a local address change.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());

  RunLoopUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_FALSE(scanner()->AllowsRandomAddressChange());
}

TEST_F(HCI_LegacyLowEnergyScannerTest, ScanUsingPublicAddress) {
  fake_address_delegate()->set_local_address(kPublicAddress1);
  EXPECT_TRUE(StartScan(false));
  RunLoopUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_EQ(LEOwnAddressType::kPublic,
            test_device()->le_scan_state().own_address_type);
}

TEST_F(HCI_LegacyLowEnergyScannerTest, ScanUsingRandomAddress) {
  fake_address_delegate()->set_local_address(kRandomAddress1);
  EXPECT_TRUE(StartScan(false));
  RunLoopUntilIdle();
  EXPECT_TRUE(scanner()->IsPassiveScanning());
  EXPECT_EQ(LEOwnAddressType::kRandom,
            test_device()->le_scan_state().own_address_type);
}

TEST_F(HCI_LegacyLowEnergyScannerTest, StopScanWhileWaitingForLocalAddress) {
  fake_address_delegate()->set_async(true);
  EXPECT_TRUE(StartScan(false));

  // Should be waiting for the random address.
  EXPECT_TRUE(scanner()->IsInitiating());
  EXPECT_TRUE(scanner()->AllowsRandomAddressChange());

  EXPECT_TRUE(scanner()->StopScan());
  RunLoopUntilIdle();

  // Should end up not scanning.
  EXPECT_TRUE(scanner()->IsIdle());
  EXPECT_FALSE(test_device()->le_scan_state().enabled);
}

}  // namespace
}  // namespace hci
}  // namespace bt
