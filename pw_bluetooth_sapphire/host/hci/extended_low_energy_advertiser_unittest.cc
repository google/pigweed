// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/extended_low_energy_advertiser.h"

#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"

namespace bt::hci {
namespace {

using bt::testing::FakeController;
using TestingBase = bt::testing::FakeDispatcherControllerTest<FakeController>;
using AdvertisingOptions = LowEnergyAdvertiser::AdvertisingOptions;
using LEAdvertisingState = FakeController::LEAdvertisingState;

constexpr AdvertisingIntervalRange kTestInterval(
    hci_spec::kLEAdvertisingIntervalMin, hci_spec::kLEAdvertisingIntervalMax);

const DeviceAddress kPublicAddress(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {2});

class ExtendedLowEnergyAdvertiserTest : public TestingBase {
 public:
  ExtendedLowEnergyAdvertiserTest() = default;
  ~ExtendedLowEnergyAdvertiserTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    // ACL data channel needs to be present for production hci::Connection
    // objects.
    TestingBase::InitializeACLDataChannel(
        hci::DataBufferInfo(),
        hci::DataBufferInfo(hci_spec::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyExtendedLEConfig();
    this->test_device()->set_settings(settings);

    advertiser_ = std::make_unique<ExtendedLowEnergyAdvertiser>(
        transport()->GetWeakPtr());
  }

  void TearDown() override {
    advertiser_ = nullptr;
    TestingBase::TearDown();
  }

  ExtendedLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

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

    EXPECT_LE(result.CalculateBlockSize(include_flags),
              hci_spec::kMaxLEAdvertisingDataLength);
    return result;
  }

  std::optional<Result<>> GetLastStatus() {
    if (!last_status_) {
      return std::nullopt;
    }

    return std::exchange(last_status_, std::nullopt).value();
  }

 private:
  std::unique_ptr<ExtendedLowEnergyAdvertiser> advertiser_;
  std::optional<Result<>> last_status_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ExtendedLowEnergyAdvertiserTest);
};

TEST_F(ExtendedLowEnergyAdvertiserTest, TxPowerLevelRetrieved) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval,
                             /*anonymous=*/false,
                             kDefaultNoAdvFlags,
                             /*include_tx_power_level=*/true);

  std::unique_ptr<LowEnergyConnection> link;
  auto conn_cb = [&link](auto cb_link) { link = std::move(cb_link); };

  this->advertiser()->StartAdvertising(kPublicAddress,
                                       ad,
                                       scan_data,
                                       options,
                                       conn_cb,
                                       this->MakeExpectSuccessCallback());
  RunUntilIdle();
  ASSERT_TRUE(this->GetLastStatus());
  EXPECT_EQ(1u, this->advertiser()->NumAdvertisements());
  EXPECT_TRUE(this->advertiser()->IsAdvertising());
  EXPECT_TRUE(this->advertiser()->IsAdvertising(kPublicAddress));

  std::optional<hci_spec::AdvertisingHandle> handle =
      this->advertiser()->LastUsedHandleForTesting();
  ASSERT_TRUE(handle);
  const LEAdvertisingState& st =
      this->test_device()->extended_advertising_state(handle.value());

  AdvertisingData::ParseResult actual_ad =
      AdvertisingData::FromBytes(st.advertised_view());
  AdvertisingData::ParseResult actual_scan_rsp =
      AdvertisingData::FromBytes(st.scan_rsp_view());

  ASSERT_EQ(fit::ok(), actual_ad);
  ASSERT_EQ(fit::ok(), actual_scan_rsp);
  EXPECT_EQ(hci_spec::kLEAdvertisingTxPowerMax, actual_ad.value().tx_power());
  EXPECT_EQ(hci_spec::kLEAdvertisingTxPowerMax,
            actual_scan_rsp.value().tx_power());
}

}  // namespace
}  // namespace bt::hci
