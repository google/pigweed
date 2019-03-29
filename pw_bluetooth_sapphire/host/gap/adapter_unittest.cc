// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"

#include <memory>

#include <lib/async/cpp/task.h>
#include <lib/zx/channel.h>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_device.h"
#include "src/lib/fxl/macros.h"

#include "low_energy_address_manager.h"
#include "low_energy_advertising_manager.h"
#include "low_energy_discovery_manager.h"

namespace bt {
namespace gap {
namespace {

using bt::testing::FakeController;
using bt::testing::FakeDevice;
using TestingBase = bt::testing::FakeControllerTest<FakeController>;

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

  FXL_DISALLOW_COPY_AND_ASSIGN(AdapterTest);
};

using GAP_AdapterTest = AdapterTest;

TEST_F(GAP_AdapterTest, InitializeFailureNoFeaturesSupported) {
  bool success;
  int init_cb_count = 0;
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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
  auto init_cb = [&, this](bool cb_success) {
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

TEST_F(GAP_AdapterTest, RemoteDeviceCacheReturnsNonNull) {
  EXPECT_TRUE(adapter()->remote_device_cache());
}

TEST_F(GAP_AdapterTest, LeAutoConnect) {
  constexpr zx::duration kTestScanPeriod = zx::sec(10);
  constexpr DeviceId kDeviceId(1234);
  const common::DeviceAddress kAddress(common::DeviceAddress::Type::kLEPublic,
                                       "00:00:00:00:00:01");

  FakeController::Settings settings;
  settings.ApplyLEOnlyDefaults();
  test_device()->set_settings(settings);

  InitializeAdapter([](bool) {});
  adapter()->le_discovery_manager()->set_scan_period(kTestScanPeriod);

  auto fake_dev = std::make_unique<FakeDevice>(kAddress, true, false);
  fake_dev->enable_directed_advertising(true);
  test_device()->AddDevice(std::move(fake_dev));

  LowEnergyConnectionRefPtr conn;
  adapter()->set_auto_connect_callback(
      [&](auto conn_ref) { conn = std::move(conn_ref); });

  // Enable background scanning. No auto-connect should take place since the
  // device isn't yet bonded.
  adapter()->le_discovery_manager()->EnableBackgroundScan(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(conn);
  EXPECT_EQ(0u, adapter()->remote_device_cache()->count());

  // Mark the device as bonded and advance the scan period.
  sm::PairingData pdata;
  pdata.ltk = sm::LTK();
  adapter()->remote_device_cache()->AddBondedDevice(kDeviceId, kAddress, pdata,
                                                    {});
  EXPECT_EQ(1u, adapter()->remote_device_cache()->count());
  RunLoopFor(kTestScanPeriod);

  // The device should have been auto-connected.
  ASSERT_TRUE(conn);
  EXPECT_EQ(kDeviceId, conn->device_identifier());
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

  // Enable privacy. The random address should not configured while advertising
  // is in progress.
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

}  // namespace
}  // namespace gap
}  // namespace bt
