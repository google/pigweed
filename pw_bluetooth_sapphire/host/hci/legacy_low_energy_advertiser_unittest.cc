// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt {

using testing::FakeController;
using testing::FakePeer;

namespace hci {

using AdvertisingOptions = LowEnergyAdvertiser::AdvertisingOptions;

namespace {

using TestingBase = bt::testing::ControllerTest<FakeController>;

const DeviceAddress kPublicAddress(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {2});

constexpr AdvertisingIntervalRange kTestInterval(hci_spec::kLEAdvertisingIntervalMin,
                                                 hci_spec::kLEAdvertisingIntervalMax);

class LegacyLowEnergyAdvertiserTest : public TestingBase {
 public:
  LegacyLowEnergyAdvertiserTest() = default;
  ~LegacyLowEnergyAdvertiserTest() override = default;

 protected:
  // TestingBase overrides:
  void SetUp() override {
    TestingBase::SetUp();

    // ACL data channel needs to be present for production hci::Connection
    // objects.
    TestingBase::InitializeACLDataChannel(hci::DataBufferInfo(),
                                          hci::DataBufferInfo(hci_spec::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    settings.bd_addr = kPublicAddress;
    test_device()->set_settings(settings);

    advertiser_ = std::make_unique<LegacyLowEnergyAdvertiser>(transport()->WeakPtr());

    StartTestDevice();
  }

  void TearDown() override {
    advertiser_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  LegacyLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

  ResultFunction<> MakeExpectSuccessCallback() {
    return [this](Result<> status) {
      last_status_ = status;
      EXPECT_EQ(fitx::ok(), status);
    };
  }

  ResultFunction<> MakeExpectErrorCallback() {
    return [this](Result<> status) {
      last_status_ = status;
      EXPECT_EQ(fitx::failed(), status);
    };
  }

  // Retrieves the last status, and resets the last status to empty.
  std::optional<Result<>> MoveLastStatus() { return std::move(last_status_); }

  // Makes some fake advertising data.
  // |include_flags| signals whether to include flag encoding size in the data calculation.
  AdvertisingData GetExampleData(bool include_flags = true) {
    AdvertisingData result;

    auto name = "fuchsia";
    EXPECT_TRUE(result.SetLocalName(name));

    auto appearance = 0x1234;
    result.SetAppearance(appearance);

    EXPECT_LE(result.CalculateBlockSize(include_flags), hci_spec::kMaxLEAdvertisingDataLength);

    return result;
  }

  // Makes fake advertising data that is too large.
  // |include_flags| signals whether to include flag encoding size in the data calculation.
  AdvertisingData GetTooLargeExampleData(bool include_tx_power, bool include_flags) {
    AdvertisingData result;

    std::string name;
    if (include_tx_power && include_flags) {
      // |name| is 24 bytes. In TLV Format, this would require 1 + 1 + 24 = 26 bytes to serialize.
      // The TX Power is encoded as 3 bytes.
      // The flags are encoded |kFlagsSize| = 3 bytes.
      // Total = 32 bytes.
      result.SetTxPower(3);
      name = "fuchsiafuchsiafuchsia123";
    } else if (!include_tx_power && !include_flags) {
      // |name| is 30 bytes. In TLV Format, this would require 32 bytes to serialize.
      name = "fuchsiafuchsiafuchsiafuchsia12";
    } else {
      if (include_tx_power)
        result.SetTxPower(3);
      // |name| 27 bytes: 29 bytes to serialize.
      // |TX Power| OR |flags|: 3 bytes to serialize.
      // Total = 32 bytes.
      name = "fuchsiafuchsiafuchsia123456";
    }
    EXPECT_TRUE(result.SetLocalName(name));

    // The maximum advertisement packet is: |hci_spec::kMaxLEAdvertisingDataLength| = 31, and
    // |result| = 32 bytes. |result| should be too large to advertise.
    EXPECT_GT(result.CalculateBlockSize(include_flags), hci_spec::kMaxLEAdvertisingDataLength);

    return result;
  }

 private:
  std::unique_ptr<LegacyLowEnergyAdvertiser> advertiser_;

  std::optional<Result<>> last_status_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyAdvertiserTest);
};

// - Rejects StartAdvertising for a different address when Advertising already
TEST_F(LegacyLowEnergyAdvertiserTest, NoAdvertiseTwice) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 MakeExpectSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);

  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));
  EXPECT_EQ(hci_spec::LEOwnAddressType::kRandom,
            test_device()->legacy_advertising_state().own_address_type);

  uint16_t new_appearance = 0x6789;
  ad.SetAppearance(new_appearance);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                 MakeExpectErrorCallback());
  RunLoopUntilIdle();

  // Should still be using the random address.
  EXPECT_EQ(hci_spec::LEOwnAddressType::kRandom,
            test_device()->legacy_advertising_state().own_address_type);
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));
}

// Tests starting and stopping an advertisement when the TX power is requested.
// Validates the advertising and scan response data are correctly populated with the
// TX power.
TEST_F(LegacyLowEnergyAdvertiserTest, StartAndStopWithTxPower) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 MakeExpectSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);

  // Verify the advertising and scan response data contains the newly populated TX Power Level.
  // See |../testing/fake_controller.cc:1585| for return value.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));

  scan_data.SetTxPower(0x9);
  DynamicByteBuffer expected_scan_rsp(ad.CalculateBlockSize(/*include_flags=*/false));
  scan_data.WriteBlock(&expected_scan_rsp, std::nullopt);
  EXPECT_TRUE(ContainersEqual(test_device()->legacy_advertising_state().scan_rsp_view(),
                              expected_scan_rsp));

  advertiser()->StopAdvertising(kRandomAddress);
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);
}

// Tests sending a second StartAdvertising command while the first one is outstanding,
// with TX power enabled.
TEST_F(LegacyLowEnergyAdvertiserTest, StartWhileStartingWithTxPower) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min() + 1, kTestInterval.max() - 1);
  AdvertisingOptions new_options(new_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                 /*include_tx_power_level=*/true);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr,
                                 MakeExpectSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->legacy_advertising_state().interval_max);

  // Verify the advertising data contains the newly populated TX Power Level.
  // Since the scan response data is empty, it's power level should not be populated.
  // See |../testing/fake_controller.cc:1585| for return value.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));
  EXPECT_TRUE(ContainersEqual(test_device()->legacy_advertising_state().scan_rsp_view(),
                              DynamicByteBuffer()));
}

// Test that the second StartAdvertising call (with no TX Power requested) successfully supercedes
// the first ongoing StartAdvertising call (with TX Power requested).
// Validates the advertised data does not include the TX power.
TEST_F(LegacyLowEnergyAdvertiserTest, StartWhileStartingTxPowerRequestedThenNotRequested) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min() + 1, kTestInterval.max() - 1);
  AdvertisingOptions new_options(new_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                 /*include_tx_power_level=*/false);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr,
                                 MakeExpectSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->legacy_advertising_state().interval_max);

  // Verify the advertising data doesn't contain a new TX Power Level.
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));
}

// Test that the second StartAdvertising call (with TX Power requested) successfully supercedes
// the first ongoing StartAdvertising call (no TX Power requested).
// Validates the advertised data includes the TX power.
TEST_F(LegacyLowEnergyAdvertiserTest, StartingWhileStartingTxPowerNotRequestedThenRequested) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);
  const AdvertisingIntervalRange new_interval(kTestInterval.min() + 1, kTestInterval.max() - 1);
  AdvertisingOptions new_options(new_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                 /*include_tx_power_level=*/true);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr,
                                 MakeExpectSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->legacy_advertising_state().interval_max);

  // Verify the advertising data doesn't contain a new TX Power Level.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->legacy_advertising_state().advertised_view(), expected_ad));
  EXPECT_TRUE(ContainersEqual(test_device()->legacy_advertising_state().scan_rsp_view(),
                              DynamicByteBuffer()));
}

// Tests that advertising gets enabled successfully with the updated parameters if
// StartAdvertising is called during a TX Power Level read.
TEST_F(LegacyLowEnergyAdvertiserTest, StartWhileTxPowerReadSuccess) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min() + 1, kTestInterval.max() - 1);
  AdvertisingOptions new_options(new_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                 /*include_tx_power_level=*/true);

  // Hold off on responding to the first TX Power Level Read command.
  test_device()->set_tx_power_level_read_response_flag(/*respond=*/false);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, MakeExpectErrorCallback());
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);

  RunLoopUntilIdle();
  // At this point in time, the first StartAdvertising call is still waiting on the TX Power Level
  // Read response.

  // Queue up the next StartAdvertising call.
  // This call should override the previous call's advertising parameters.
  test_device()->set_tx_power_level_read_response_flag(/*respond=*/true);
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr,
                                 MakeExpectSuccessCallback());

  // Explicitly respond to the first TX Power Level read command.
  test_device()->OnLEReadAdvertisingChannelTxPower();

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->legacy_advertising_state().interval_max);
}

// Tests that advertising does not get enabled if the TX Power read fails.
TEST_F(LegacyLowEnergyAdvertiserTest, StartAdvertisingReadTxPowerFails) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  // Simulate failure for Read TX Power operation.
  test_device()->SetDefaultResponseStatus(hci_spec::kLEReadAdvertisingChannelTxPower,
                                          hci_spec::StatusCode::kHardwareFailure);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 MakeExpectErrorCallback());
  RunLoopUntilIdle();
  auto status = MoveLastStatus();
  ASSERT_TRUE(status.has_value());
  ASSERT_TRUE(status->is_error());
  EXPECT_TRUE(status->error_value().is_protocol_error());
}

// TODO(create bug): This test should really belong in LowEnergyAdvertiser's unittest file
// (//src/connectivity/bluetooth/core/bt-host/hci/low_energy_advertiser_unittest.cc) because all low
// energy advertisers should follow this convention. However, this requires that all low energy
// advertisers implement random address rotation. Currently, the only other low energy advertiser
// is the ExtendedLowEnergyAdvertiser. For ExtendedLowEnergyAdvertiser, we will implement random
// address rotation in a future project. When that is done, we should move this test to the general
// LowEnergyAdvertiser unit test file.
TEST_F(LegacyLowEnergyAdvertiserTest, AllowsRandomAddressChange) {
  AdvertisingData scan_rsp;
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  // The random address can be changed while not advertising.
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());

  // The random address cannot be changed while starting to advertise.
  advertiser()->StartAdvertising(kRandomAddress, GetExampleData(), scan_rsp, options, nullptr,
                                 MakeExpectSuccessCallback());
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);
  EXPECT_FALSE(advertiser()->AllowsRandomAddressChange());

  // The random address cannot be changed while advertising is enabled.
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_FALSE(advertiser()->AllowsRandomAddressChange());

  // The advertiser allows changing the address while advertising is getting
  // stopped.
  advertiser()->StopAdvertising(kRandomAddress);
  EXPECT_TRUE(test_device()->legacy_advertising_state().enabled);
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());

  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());
}

TEST_F(LegacyLowEnergyAdvertiserTest, StopWhileStarting) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                       MakeExpectErrorCallback());
  this->advertiser()->StopAdvertising(kPublicAddress);

  this->RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_FALSE(test_device()->legacy_advertising_state().enabled);
}

}  // namespace
}  // namespace hci
}  // namespace bt
