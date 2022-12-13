// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/android_extended_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/extended_low_energy_advertiser.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"

// Multiple advertising is supported by the Bluetooth 5.0+ Core Specification as well as Android
// vendor extensions. This test file contains shared tests for both versions of LE Multiple
// Advertising.

namespace bt::hci {
namespace {

using bt::testing::FakeController;
using TestingBase = bt::testing::ControllerTest<FakeController>;
using AdvertisingOptions = LowEnergyAdvertiser::AdvertisingOptions;
using LEAdvertisingState = FakeController::LEAdvertisingState;

constexpr AdvertisingIntervalRange kTestInterval(hci_spec::kLEAdvertisingIntervalMin,
                                                 hci_spec::kLEAdvertisingIntervalMax);

const DeviceAddress kPublicAddress(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {2});

template <typename T>
class LowEnergyMultipleAdvertisingTest : public TestingBase {
 public:
  LowEnergyMultipleAdvertisingTest() = default;
  ~LowEnergyMultipleAdvertisingTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    // ACL data channel needs to be present for production hci::Connection objects.
    TestingBase::InitializeACLDataChannel(hci::DataBufferInfo(),
                                          hci::DataBufferInfo(hci_spec::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyExtendedLEConfig();
    this->test_device()->set_settings(settings);

    advertiser_ = std::unique_ptr<T>(CreateAdvertiserInternal());
  }

  void TearDown() override {
    advertiser_ = nullptr;
    this->test_device()->Stop();
    TestingBase::TearDown();
  }

  template <bool same = std::is_same_v<T, AndroidExtendedLowEnergyAdvertiser>>
  std::enable_if_t<same, AndroidExtendedLowEnergyAdvertiser>* CreateAdvertiserInternal() {
    return new AndroidExtendedLowEnergyAdvertiser(transport()->WeakPtr(), max_advertisements_);
  }

  template <bool same = std::is_same_v<T, ExtendedLowEnergyAdvertiser>>
  std::enable_if_t<same, ExtendedLowEnergyAdvertiser>* CreateAdvertiserInternal() {
    return new ExtendedLowEnergyAdvertiser(transport()->WeakPtr());
  }

  T* advertiser() const { return advertiser_.get(); }

  ResultFunction<> MakeExpectSuccessCallback() {
    return [this](Result<> status) {
      last_status_ = status;
      EXPECT_EQ(fit::ok(), status);
    };
  }

  ResultFunction<> MakeExpectErrorCallback() {
    return [this](Result<> status) {
      last_status_ = status;
      EXPECT_EQ(fit::failed(), status);
    };
  }

  static AdvertisingData GetExampleData(bool include_flags = true) {
    AdvertisingData result;

    std::string name = "fuchsia";
    EXPECT_TRUE(result.SetLocalName(name));

    uint16_t appearance = 0x1234;
    result.SetAppearance(appearance);

    EXPECT_LE(result.CalculateBlockSize(include_flags), hci_spec::kMaxLEAdvertisingDataLength);
    return result;
  }

  std::optional<Result<>> GetLastStatus() {
    if (!last_status_) {
      return std::nullopt;
    }

    return std::exchange(last_status_, std::nullopt).value();
  }

  uint8_t max_advertisements() const { return max_advertisements_; }

 private:
  std::unique_ptr<T> advertiser_;
  std::optional<Result<>> last_status_;
  uint8_t max_advertisements_ = 2;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyMultipleAdvertisingTest);
};

using Implementations =
    ::testing::Types<ExtendedLowEnergyAdvertiser, AndroidExtendedLowEnergyAdvertiser>;
TYPED_TEST_SUITE(LowEnergyMultipleAdvertisingTest, Implementations);

TYPED_TEST(LowEnergyMultipleAdvertisingTest, LegacyPduLength) {
  EXPECT_EQ(hci_spec::kMaxLEAdvertisingDataLength, this->advertiser()->GetSizeLimit());
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, AdvertisingHandlesExhausted) {
  this->test_device()->set_num_supported_advertising_sets(this->max_advertisements());

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  for (uint8_t i = 0; i < this->max_advertisements(); i++) {
    this->advertiser()->StartAdvertising(DeviceAddress(DeviceAddress::Type::kLEPublic, {i}), ad,
                                         scan_data, options, /*connect_callback=*/nullptr,
                                         this->MakeExpectSuccessCallback());
    this->RunLoopUntilIdle();
  }

  ASSERT_TRUE(this->GetLastStatus());
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(this->max_advertisements(), this->advertiser()->NumAdvertisements());

  this->advertiser()->StartAdvertising(
      DeviceAddress(DeviceAddress::Type::kLEPublic, {hci_spec::kAdvertisingHandleMax + 1}), ad,
      scan_data, options, /*connect_callback=*/nullptr, this->MakeExpectErrorCallback());

  this->RunLoopUntilIdle();
  ASSERT_TRUE(this->GetLastStatus());
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(this->max_advertisements(), this->advertiser()->NumAdvertisements());
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, SimultaneousAdvertisements) {
  this->test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_public_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(hci_spec::kLEAdvertisingIntervalMin + 1u,
                                                     hci_spec::kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_random_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_EQ(2u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kRandomAddress));

  const LEAdvertisingState& public_addr_state =
      this->test_device()->extended_advertising_state(handle_public_addr.value());
  const LEAdvertisingState& random_addr_state =
      this->test_device()->extended_advertising_state(handle_random_addr.value());

  EXPECT_TRUE(public_addr_state.enabled);
  EXPECT_TRUE(random_addr_state.enabled);
  EXPECT_EQ(hci_spec::LEOwnAddressType::kPublic, public_addr_state.own_address_type);
  EXPECT_EQ(hci_spec::LEOwnAddressType::kRandom, random_addr_state.own_address_type);
  EXPECT_EQ(hci_spec::kLEAdvertisingIntervalMin, public_addr_state.interval_min);
  EXPECT_EQ(hci_spec::kLEAdvertisingIntervalMax, public_addr_state.interval_max);
  EXPECT_EQ(hci_spec::kLEAdvertisingIntervalMin + 1u, random_addr_state.interval_min);
  EXPECT_EQ(hci_spec::kLEAdvertisingIntervalMax - 1u, random_addr_state.interval_max);
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, StopAdvertisingAllAdvertisementsStopped) {
  this->test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_public_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(hci_spec::kLEAdvertisingIntervalMin + 1u,
                                                     hci_spec::kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_random_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_EQ(2u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kRandomAddress));

  // Stop advertising
  this->advertiser()->StopAdvertising();
  this->RunLoopUntilIdle();

  // Check that advertiser and controller both report not advertising
  EXPECT_EQ(0u, this->advertiser()->NumAdvertisements());
  EXPECT_FALSE(this->advertiser()->IsAdvertising());
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kRandomAddress));

  const LEAdvertisingState& public_addr_state =
      this->test_device()->extended_advertising_state(handle_public_addr.value());
  const LEAdvertisingState& random_addr_state =
      this->test_device()->extended_advertising_state(handle_random_addr.value());

  constexpr uint8_t blank[hci_spec::kMaxLEAdvertisingDataLength] = {0};

  EXPECT_FALSE(public_addr_state.enabled);
  EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, public_addr_state.data_length);
  EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, public_addr_state.scan_rsp_length);

  EXPECT_FALSE(random_addr_state.enabled);
  EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, random_addr_state.data_length);
  EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
  EXPECT_EQ(0, random_addr_state.scan_rsp_length);
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, StopAdvertisingSingleAdvertisement) {
  this->test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();

  // start public address advertising
  AdvertisingOptions public_options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, public_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_public_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_public_addr);

  // start random address advertising
  constexpr AdvertisingIntervalRange random_interval(hci_spec::kLEAdvertisingIntervalMin + 1u,
                                                     hci_spec::kLEAdvertisingIntervalMax - 1u);
  AdvertisingOptions random_options(random_interval, /*anonymous=*/false, kDefaultNoAdvFlags,
                                    /*include_tx_power_level=*/false);
  this->advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, random_options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->RunLoopUntilIdle();
  std::optional<hci_spec::AdvertisingHandle> handle_random_addr =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle_random_addr);

  // check everything is correct
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(2u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kRandomAddress));

  // Stop advertising the random address
  this->advertiser()->StopAdvertising(kRandomAddress);
  this->RunLoopUntilIdle();

  // Check that advertiser and controller both report the same advertising state
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(1u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kRandomAddress));

  constexpr uint8_t blank[hci_spec::kMaxLEAdvertisingDataLength] = {0};

  {
    const LEAdvertisingState& public_addr_state =
        this->test_device()->extended_advertising_state(handle_public_addr.value());
    const LEAdvertisingState& random_addr_state =
        this->test_device()->extended_advertising_state(handle_random_addr.value());

    EXPECT_TRUE(public_addr_state.enabled);
    EXPECT_NE(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_NE(0, public_addr_state.data_length);
    EXPECT_NE(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_NE(0, public_addr_state.scan_rsp_length);

    EXPECT_FALSE(random_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.scan_rsp_length);
  }

  // stop advertising the public address
  this->advertiser()->StopAdvertising(kPublicAddress);
  this->RunLoopUntilIdle();

  {
    const LEAdvertisingState& public_addr_state =
        this->test_device()->extended_advertising_state(handle_public_addr.value());
    const LEAdvertisingState& random_addr_state =
        this->test_device()->extended_advertising_state(handle_random_addr.value());

    // Check that advertiser and controller both report the same advertising state
    EXPECT_FALSE(this->advertiser()->IsAdvertising());
    EXPECT_EQ(0u, this->advertiser()->NumAdvertisements());
    EXPECT_FALSE(this->advertiser()->IsAdvertising(kPublicAddress));
    EXPECT_FALSE(this->advertiser()->IsAdvertising(kRandomAddress));

    EXPECT_FALSE(public_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, public_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, public_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, public_addr_state.scan_rsp_length);

    EXPECT_FALSE(random_addr_state.enabled);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.data_length);
    EXPECT_EQ(0, std::memcmp(blank, random_addr_state.data, hci_spec::kMaxLEAdvertisingDataLength));
    EXPECT_EQ(0, random_addr_state.scan_rsp_length);
  }
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, SuccessiveAdvertisingCalls) {
  this->test_device()->set_num_supported_advertising_sets(2);

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());

  this->RunLoopUntilIdle();
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(2u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kRandomAddress));

  this->advertiser()->StopAdvertising(kPublicAddress);
  this->advertiser()->StopAdvertising(kRandomAddress);

  this->RunLoopUntilIdle();
  EXPECT_FALSE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(0u, this->advertiser()->NumAdvertisements());
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kRandomAddress));
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, InterleavedAdvertisingCalls) {
  this->test_device()->set_num_supported_advertising_sets(this->max_advertisements());

  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->advertiser()->StopAdvertising(kPublicAddress);
  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());

  this->RunLoopUntilIdle();
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_EQ(1u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));
  EXPECT_FALSE(this->advertiser()->IsAdvertising(kRandomAddress));
}

TYPED_TEST(LowEnergyMultipleAdvertisingTest, StopWhileStarting) {
  AdvertisingData ad = this->GetExampleData();
  AdvertisingData scan_data = this->GetExampleData();
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/false);

  this->advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options,
                                       /*connect_callback=*/nullptr,
                                       this->MakeExpectSuccessCallback());
  this->advertiser()->StopAdvertising(kPublicAddress);

  this->RunLoopUntilIdle();
  EXPECT_TRUE(this->GetLastStatus());

  std::optional<hci_spec::AdvertisingHandle> handle =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle);

  EXPECT_FALSE(this->test_device()->extended_advertising_state(handle.value()).enabled);
}

}  // namespace
}  // namespace bt::hci
