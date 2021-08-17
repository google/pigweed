// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/extended_low_energy_advertiser.h"

#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"

namespace bt::hci {
namespace {

using testing::FakeController;
using TestingBase = bt::testing::ControllerTest<FakeController>;
using AdvertisingOptions = LowEnergyAdvertiser::AdvertisingOptions;
using LEAdvertisingState = FakeController::LEAdvertisingState;

constexpr AdvertisingIntervalRange kTestInterval(kLEAdvertisingIntervalMin,
                                                 kLEAdvertisingIntervalMax);

const DeviceAddress kPublicAddress(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {2});

class ExtendedLowEnergyAdvertiserTest : public TestingBase {
 public:
  ExtendedLowEnergyAdvertiserTest() = default;
  ~ExtendedLowEnergyAdvertiserTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    // ACL data channel needs to be present for production hci::Connection objects.
    TestingBase::InitializeACLDataChannel(hci::DataBufferInfo(),
                                          hci::DataBufferInfo(hci::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyLEConfig();
    test_device()->set_settings(settings);

    advertiser_ = std::make_unique<ExtendedLowEnergyAdvertiser>(transport()->WeakPtr());

    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    advertiser_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  ExtendedLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

  StatusCallback GetSuccessCallback() {
    return [this](Status status) {
      last_status_ = status;
      EXPECT_TRUE(status) << status.ToString();
    };
  }

  StatusCallback GetErrorCallback() {
    return [this](Status status) {
      last_status_ = status;
      EXPECT_FALSE(status);
    };
  }

  static AdvertisingData GetExampleData(bool include_flags = true) {
    AdvertisingData result;

    std::string name = "fuchsia";
    EXPECT_TRUE(result.SetLocalName(name));

    uint16_t appearance = 0x1234;
    result.SetAppearance(appearance);

    EXPECT_LE(result.CalculateBlockSize(include_flags), kMaxLEAdvertisingDataLength);
    return result;
  }

  std::optional<Status> GetLastStatus() {
    if (!last_status_) {
      return std::nullopt;
    }

    Status status = last_status_.value();
    last_status_.reset();
    return status;
  }

 private:
  std::unique_ptr<ExtendedLowEnergyAdvertiser> advertiser_;
  std::optional<Status> last_status_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ExtendedLowEnergyAdvertiserTest);
};

TEST_F(ExtendedLowEnergyAdvertiserTest, LegacyPduLength) {
  EXPECT_EQ(kMaxLEAdvertisingDataLength, advertiser()->GetSizeLimit());
}

TEST_F(ExtendedLowEnergyAdvertiserTest, AdvertisingHandlesExhausted) {
  test_device()->set_num_supported_advertising_sets(AdvertisingHandleMap::kMaxElements);

  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  for (uint8_t i = 0; i <= kAdvertisingHandleMax; i++) {
    advertiser()->StartAdvertising(DeviceAddress(DeviceAddress::Type::kLEPublic, {i}), ad,
                                   scan_data, options, nullptr, GetSuccessCallback());
    RunLoopUntilIdle();
  }

  ASSERT_TRUE(GetLastStatus());
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(AdvertisingHandleMap::kMaxElements, advertiser()->NumAdvertisements());

  advertiser()->StartAdvertising(
      DeviceAddress(DeviceAddress::Type::kLEPublic, {kAdvertisingHandleMax + 1}), ad, scan_data,
      options, nullptr, GetErrorCallback());

  RunLoopUntilIdle();
  ASSERT_TRUE(GetLastStatus());
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(AdvertisingHandleMap::kMaxElements, advertiser()->NumAdvertisements());
}

TEST_F(ExtendedLowEnergyAdvertiserTest, TxPowerLevelRetrieved) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  ConnectionPtr link;
  auto conn_cb = [&link](auto cb_link) { link = std::move(cb_link); };

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, conn_cb,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(GetLastStatus());
  EXPECT_EQ(1u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));

  std::optional<AdvertisingHandle> handle = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle);
  const LEAdvertisingState& st = test_device()->extended_advertising_state(handle.value());

  std::optional<AdvertisingData> actual_ad = AdvertisingData::FromBytes(st.advertised_view());
  std::optional<AdvertisingData> actual_scan_rsp = AdvertisingData::FromBytes(st.scan_rsp_view());

  ASSERT_TRUE(actual_ad);
  ASSERT_TRUE(actual_scan_rsp);
  EXPECT_EQ(kLEAdvertisingTxPowerMax, actual_ad.value().tx_power());
  EXPECT_EQ(kLEAdvertisingTxPowerMax, actual_scan_rsp.value().tx_power());
}

TEST_F(ExtendedLowEnergyAdvertiserTest, SimultaneousAdvertisements) {
  test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_public_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(kLEAdvertisingIntervalMin + 1u,
                                                     kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_random_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_EQ(2u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(advertiser()->IsAdvertising(kRandomAddress));

  const LEAdvertisingState& public_addr_state =
      test_device()->extended_advertising_state(handle_public_addr.value());
  const LEAdvertisingState& random_addr_state =
      test_device()->extended_advertising_state(handle_random_addr.value());

  EXPECT_TRUE(public_addr_state.enabled);
  EXPECT_TRUE(random_addr_state.enabled);
  EXPECT_EQ(LEOwnAddressType::kPublic, public_addr_state.own_address_type);
  EXPECT_EQ(LEOwnAddressType::kRandom, random_addr_state.own_address_type);
  EXPECT_EQ(kLEAdvertisingIntervalMin, public_addr_state.interval_min);
  EXPECT_EQ(kLEAdvertisingIntervalMax, public_addr_state.interval_max);
  EXPECT_EQ(kLEAdvertisingIntervalMin + 1u, random_addr_state.interval_min);
  EXPECT_EQ(kLEAdvertisingIntervalMax - 1u, random_addr_state.interval_max);
}

TEST_F(ExtendedLowEnergyAdvertiserTest, StopAdvertisingAllAdvertisementsStopped) {
  test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_public_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(kLEAdvertisingIntervalMin + 1u,
                                                     kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_random_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_EQ(2u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(advertiser()->IsAdvertising(kRandomAddress));

  // Stop advertising
  advertiser()->StopAdvertising();
  RunLoopUntilIdle();

  // Check that advertiser and controller both report not advertising
  EXPECT_EQ(0u, advertiser()->NumAdvertisements());
  EXPECT_FALSE(advertiser()->IsAdvertising());
  EXPECT_FALSE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(advertiser()->IsAdvertising(kRandomAddress));

  const LEAdvertisingState& public_addr_state =
      test_device()->extended_advertising_state(handle_public_addr.value());
  const LEAdvertisingState& random_addr_state =
      test_device()->extended_advertising_state(handle_random_addr.value());

  constexpr uint8_t blank[kMaxLEAdvertisingDataLength] = {0};

  EXPECT_FALSE(public_addr_state.enabled);
  EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, public_addr_state.data_length);
  EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, public_addr_state.scan_rsp_length);

  EXPECT_FALSE(random_addr_state.enabled);
  EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, random_addr_state.data_length);
  EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, random_addr_state.scan_rsp_length);
}

TEST_F(ExtendedLowEnergyAdvertiserTest, StopAdvertisingSingleAdvertisement) {
  test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_public_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(kLEAdvertisingIntervalMin + 1u,
                                                     kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  std::optional<AdvertisingHandle> handle_random_addr = advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(2u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(advertiser()->IsAdvertising(kRandomAddress));

  // Stop advertising the random address
  advertiser()->StopAdvertising(kRandomAddress);
  RunLoopUntilIdle();

  // Check that advertiser and controller both report the same advertising state
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(1u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(advertiser()->IsAdvertising(kRandomAddress));

  constexpr uint8_t blank[kMaxLEAdvertisingDataLength] = {0};

  {
    const LEAdvertisingState& public_addr_state =
        test_device()->extended_advertising_state(handle_public_addr.value());
    const LEAdvertisingState& random_addr_state =
        test_device()->extended_advertising_state(handle_random_addr.value());

    EXPECT_TRUE(public_addr_state.enabled);
    EXPECT_NE(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_NE(0, public_addr_state.data_length);
    EXPECT_NE(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_NE(0, public_addr_state.scan_rsp_length);

    EXPECT_FALSE(random_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.scan_rsp_length);
  }

  // stop advertising the public address
  advertiser()->StopAdvertising(kPublicAddress);
  RunLoopUntilIdle();

  {
    const LEAdvertisingState& public_addr_state =
        test_device()->extended_advertising_state(handle_public_addr.value());
    const LEAdvertisingState& random_addr_state =
        test_device()->extended_advertising_state(handle_random_addr.value());

    // Check that advertiser and controller both report the same advertising state
    EXPECT_FALSE(advertiser()->IsAdvertising());
    EXPECT_EQ(0u, advertiser()->NumAdvertisements());
    EXPECT_FALSE(advertiser()->IsAdvertising(kPublicAddress));
    EXPECT_FALSE(advertiser()->IsAdvertising(kRandomAddress));

    EXPECT_FALSE(public_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, public_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, public_addr_state.scan_rsp_length);

    EXPECT_FALSE(random_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.scan_rsp_length);
  }
}

TEST_F(ExtendedLowEnergyAdvertiserTest, SuccessiveAdvertisingCalls) {
  test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());

  RunLoopUntilIdle();
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(2u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(advertiser()->IsAdvertising(kRandomAddress));

  advertiser()->StopAdvertising(kPublicAddress);
  advertiser()->StopAdvertising(kRandomAddress);

  RunLoopUntilIdle();
  EXPECT_FALSE(advertiser()->IsAdvertising());
  EXPECT_EQ(0u, advertiser()->NumAdvertisements());
  EXPECT_FALSE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(advertiser()->IsAdvertising(kRandomAddress));
}

TEST_F(ExtendedLowEnergyAdvertiserTest, InterleavedAdvertisingCalls) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  advertiser()->StopAdvertising(kPublicAddress);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());

  RunLoopUntilIdle();
  EXPECT_TRUE(advertiser()->IsAdvertising());
  EXPECT_EQ(1u, advertiser()->NumAdvertisements());
  EXPECT_TRUE(advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(advertiser()->IsAdvertising(kRandomAddress));
}

}  // namespace
}  // namespace bt::hci
