// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"

#include <zircon/assert.h>
#include <zircon/syscalls.h>

#include <map>

#include <fbl/macros.h>

#include "lib/fitx/internal/result.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt {
using testing::FakeController;

namespace gap {
namespace {
using TestingBase = bt::testing::ControllerTest<FakeController>;

constexpr size_t kDefaultMaxAdSize = 23;
constexpr size_t kDefaultFakeAdSize = 20;
constexpr AdvertisingInterval kTestInterval = AdvertisingInterval::FAST1;

const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom,
                                   {0x55, 0x44, 0x33, 0x22, 0x11, 0x00});

void NopConnectCallback(AdvertisementId, std::unique_ptr<hci::Connection>) {}

struct AdvertisementStatus {
  AdvertisingData data;
  AdvertisingData scan_rsp;
  bool anonymous;
  uint16_t interval_min;
  uint16_t interval_max;
  hci::LowEnergyAdvertiser::ConnectionCallback connect_cb;
};

// LowEnergyAdvertiser for testing purposes:
//  - Reports mas_ad_size supported
//  - Actually just accepts all ads and stores them in ad_store
class FakeLowEnergyAdvertiser final : public hci::LowEnergyAdvertiser {
 public:
  FakeLowEnergyAdvertiser(fxl::WeakPtr<hci::Transport> hci, size_t max_ad_size,
                          std::unordered_map<DeviceAddress, AdvertisementStatus>* ad_store)
      : hci::LowEnergyAdvertiser(std::move(hci)), max_ad_size_(max_ad_size), ads_(ad_store) {
    ZX_ASSERT(ads_);
  }

  ~FakeLowEnergyAdvertiser() override = default;

  size_t GetSizeLimit() const override { return max_ad_size_; }

  bool AllowsRandomAddressChange() const override { return true; }

  void StartAdvertising(const DeviceAddress& address, const AdvertisingData& data,
                        const AdvertisingData& scan_rsp, AdvertisingOptions adv_options,
                        ConnectionCallback connect_callback,
                        hci::ResultFunction<> callback) override {
    if (pending_error_.is_error()) {
      callback(pending_error_);
      pending_error_ = fitx::ok();
      return;
    }
    if (data.CalculateBlockSize(/*include_flags=*/true) > max_ad_size_) {
      callback(ToResult(HostError::kInvalidParameters));
      return;
    }
    if (scan_rsp.CalculateBlockSize(/*include_flags=*/false) > max_ad_size_) {
      callback(ToResult(HostError::kInvalidParameters));
      return;
    }
    AdvertisementStatus new_status;
    data.Copy(&new_status.data);
    scan_rsp.Copy(&new_status.scan_rsp);
    new_status.connect_cb = std::move(connect_callback);
    new_status.interval_min = adv_options.interval.min();
    new_status.interval_max = adv_options.interval.max();
    new_status.anonymous = adv_options.anonymous;
    ads_->emplace(address, std::move(new_status));
    callback(fitx::ok());
  }

  void StopAdvertising(const DeviceAddress& address) override { ads_->erase(address); }

  void OnIncomingConnection(hci_spec::ConnectionHandle handle, hci::Connection::Role role,
                            const DeviceAddress& peer_address,
                            const hci_spec::LEConnectionParameters& conn_params) override {
    // Right now, we call the first callback, because we can't call any other
    // ones.
    // TODO(jamuraa): make this send it to the correct callback once we can
    // determine which one that is.
    const auto& cb = ads_->begin()->second.connect_cb;
    if (cb) {
      cb(std::make_unique<hci::testing::FakeConnection>(handle, bt::LinkType::kLE, role,
                                                        ads_->begin()->first, peer_address));
    }
  }

  // Sets this faker up to send an error back from the next StartAdvertising
  // call. Set to success to disable a previously called error.
  void ErrorOnNext(hci::Result<> error_status) { pending_error_ = error_status; }

 private:
  std::unique_ptr<hci::CommandPacket> BuildEnablePacket(
      const DeviceAddress& address, hci_spec::GenericEnableParam enable) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildSetAdvertisingParams(
      const DeviceAddress& address, hci_spec::LEAdvertisingType type,
      hci_spec::LEOwnAddressType own_address_type,
      hci::AdvertisingIntervalRange interval) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildSetAdvertisingData(const DeviceAddress& address,
                                                              const AdvertisingData& data,
                                                              AdvFlags flags) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildUnsetAdvertisingData(
      const DeviceAddress& address) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildSetScanResponse(
      const DeviceAddress& address, const AdvertisingData& scan_rsp) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildUnsetScanResponse(
      const DeviceAddress& address) override {
    return nullptr;
  }

  std::unique_ptr<hci::CommandPacket> BuildRemoveAdvertisingSet(
      const DeviceAddress& address) override {
    return nullptr;
  }

  size_t max_ad_size_;
  std::unordered_map<DeviceAddress, AdvertisementStatus>* ads_;
  hci::Result<> pending_error_ = fitx::ok();

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeLowEnergyAdvertiser);
};

class LowEnergyAdvertisingManagerTest : public TestingBase {
 public:
  LowEnergyAdvertisingManagerTest() = default;
  ~LowEnergyAdvertisingManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    fake_address_delegate_.set_local_address(kRandomAddress);
    MakeFakeAdvertiser();
    MakeAdvertisingManager();
  }

  void TearDown() override {
    adv_mgr_ = nullptr;
    advertiser_ = nullptr;
    TestingBase::TearDown();
  }

  // Makes some fake advertising data of a specific |packed_size|
  AdvertisingData CreateFakeAdvertisingData(size_t packed_size = kDefaultFakeAdSize) {
    AdvertisingData result;
    auto buffer = CreateStaticByteBuffer(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);
    size_t bytes_left = packed_size;
    while (bytes_left > 0) {
      // Each field to take 10 bytes total, unless the next header (4 bytes)
      // won't fit. In which case we add enough bytes to finish up.
      size_t data_bytes = bytes_left < 14 ? (bytes_left - 4) : 6;
      EXPECT_TRUE(result.SetManufacturerData(0xb000 + bytes_left, buffer.view(0, data_bytes)));
      bytes_left = packed_size - result.CalculateBlockSize();
    }
    return result;
  }

  LowEnergyAdvertisingManager::AdvertisingStatusCallback GetErrorCallback() {
    return [this](AdvertisementInstance instance, hci::Result<> status) {
      EXPECT_EQ(kInvalidAdvertisementId, instance.id());
      EXPECT_TRUE(status.is_error());
      last_status_ = status;
    };
  }

  LowEnergyAdvertisingManager::AdvertisingStatusCallback GetSuccessCallback() {
    return [this](AdvertisementInstance instance, hci::Result<> status) {
      EXPECT_NE(kInvalidAdvertisementId, instance.id());
      EXPECT_TRUE(status.is_ok());
      last_instance_ = std::move(instance);
      last_status_ = status;
    };
  }

  void MakeFakeAdvertiser(size_t max_ad_size = kDefaultMaxAdSize) {
    advertiser_ =
        std::make_unique<FakeLowEnergyAdvertiser>(transport()->WeakPtr(), max_ad_size, &ad_store_);
  }

  void MakeAdvertisingManager() {
    adv_mgr_ = std::make_unique<LowEnergyAdvertisingManager>(advertiser(), &fake_address_delegate_);
  }

  LowEnergyAdvertisingManager* adv_mgr() const { return adv_mgr_.get(); }
  const std::unordered_map<DeviceAddress, AdvertisementStatus>& ad_store() { return ad_store_; }
  AdvertisementId last_ad_id() const { return last_instance_.id(); }

  // Returns the currently active advertising state. This is useful for tests that want to verify
  // advertising parameters when there is a single known advertisement. Returns nullptr if
  // there no or more than one advertisment.
  const AdvertisementStatus* current_adv() const {
    if (ad_store_.size() != 1u) {
      return nullptr;
    }
    return &ad_store_.begin()->second;
  }

  // Returns and clears the last callback status. This resets the state to
  // detect another callback.
  const std::optional<hci::Result<>> MoveLastStatus() { return std::move(last_status_); }

  FakeLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

 private:
  hci::FakeLocalAddressDelegate fake_address_delegate_;

  // TODO(armansito): The address mapping is currently broken since the gap::LEAM always assigns the
  // controller random address. Make this track each instance by instance ID instead once the
  // layering issues have been fixed.
  std::unordered_map<DeviceAddress, AdvertisementStatus> ad_store_;
  AdvertisementInstance last_instance_;
  std::optional<hci::Result<>> last_status_;
  std::unique_ptr<FakeLowEnergyAdvertiser> advertiser_;
  std::unique_ptr<LowEnergyAdvertisingManager> adv_mgr_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyAdvertisingManagerTest);
};

// Tests:
//  - When the advertiser succeeds, the callback is called with the success
TEST_F(LowEnergyAdvertisingManagerTest, Success) {
  EXPECT_FALSE(adv_mgr()->advertising());
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), nullptr,
                              kTestInterval, /*anonymous=*/false, /*include_tx_power_level*/ false,
                              GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  ASSERT_EQ(1u, ad_store().size());
  EXPECT_TRUE(adv_mgr()->advertising());

  // Verify that the advertiser uses the requested local address.
  EXPECT_EQ(kRandomAddress, ad_store().begin()->first);
}

TEST_F(LowEnergyAdvertisingManagerTest, DataSize) {
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), nullptr,
                              kTestInterval, /*anonymous=*/false, /*include_tx_power_level*/ false,
                              GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(kDefaultMaxAdSize + 1), AdvertisingData(),
                              nullptr, kTestInterval, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetErrorCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());
}

// TODO(fxbug.dev/1335): Revise this test to use multiple advertising instances when
// multi-advertising is supported.
//  - Stopping one that is registered stops it in the advertiser
//    (and stops the right address)
//  - Stopping an advertisement that isn't registered returns false
TEST_F(LowEnergyAdvertisingManagerTest, RegisterUnregister) {
  EXPECT_FALSE(adv_mgr()->StopAdvertising(kInvalidAdvertisementId));

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), nullptr,
                              kTestInterval, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());
  EXPECT_TRUE(adv_mgr()->advertising());

  EXPECT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));
  EXPECT_TRUE(ad_store().empty());
  EXPECT_FALSE(adv_mgr()->advertising());

  EXPECT_FALSE(adv_mgr()->StopAdvertising(last_ad_id()));
  EXPECT_TRUE(ad_store().empty());
}

//  - When the advertiser returns an error, we return an error
TEST_F(LowEnergyAdvertisingManagerTest, AdvertiserError) {
  advertiser()->ErrorOnNext(ToResult(hci_spec::kInvalidHCICommandParameters));

  EXPECT_FALSE(adv_mgr()->advertising());
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), nullptr,
                              kTestInterval, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetErrorCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_FALSE(adv_mgr()->advertising());
}

//  - It calls the connectable callback correctly when connected to
TEST_F(LowEnergyAdvertisingManagerTest, ConnectCallback) {
  hci::ConnectionPtr link;
  AdvertisementId advertised_id = kInvalidAdvertisementId;

  auto connect_cb = [&](AdvertisementId connected_id, hci::ConnectionPtr cb_link) {
    link = std::move(cb_link);
    EXPECT_EQ(advertised_id, connected_id);
  };
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), connect_cb,
                              kTestInterval, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  advertised_id = last_ad_id();

  DeviceAddress peer_address(DeviceAddress::Type::kLEPublic, {3, 2, 1, 1, 2, 3});
  advertiser()->OnIncomingConnection(1, hci::Connection::Role::kPeripheral, peer_address,
                                     hci_spec::LEConnectionParameters());
  RunLoopUntilIdle();
  ASSERT_TRUE(link);

  // Make sure that the link has the correct local and peer addresses assigned.
  EXPECT_EQ(kRandomAddress, link->local_address());
  EXPECT_EQ(peer_address, link->peer_address());
}

//  - Error: Connectable and Anonymous at the same time
TEST_F(LowEnergyAdvertisingManagerTest, ConnectAdvertiseError) {
  auto connect_cb = [](AdvertisementId connected_id, hci::ConnectionPtr conn) {};

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(), AdvertisingData(), connect_cb,
                              kTestInterval, true /* anonymous */, /*include_tx_power_level*/ false,
                              GetErrorCallback());

  EXPECT_TRUE(MoveLastStatus());
}

// Passes the values for the data on. (anonymous, data, scan_rsp)
TEST_F(LowEnergyAdvertisingManagerTest, SendsCorrectData) {
  adv_mgr()->StartAdvertising(
      CreateFakeAdvertisingData(), CreateFakeAdvertisingData(21 /* size of ad */), nullptr,
      kTestInterval, false /* anonymous */, /*include_tx_power_level*/ false, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());

  auto ad_status = &ad_store().begin()->second;

  AdvertisingData expected_ad = CreateFakeAdvertisingData();
  AdvertisingData expected_scan_rsp = CreateFakeAdvertisingData(21 /* size of ad */);
  EXPECT_EQ(expected_ad, ad_status->data);
  EXPECT_EQ(expected_scan_rsp, ad_status->scan_rsp);
  EXPECT_EQ(false, ad_status->anonymous);
  EXPECT_EQ(nullptr, ad_status->connect_cb);
}

// Test that the AdvertisingInterval values map to the spec defined constants (NOTE: this might
// change in the future in favor of a more advanced policy for managing the intervals; for now they
// get mapped to recommended values from Vol 3, Part C, Appendix A).
TEST_F(LowEnergyAdvertisingManagerTest, ConnectableAdvertisingIntervals) {
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), NopConnectCallback,
                              AdvertisingInterval::FAST1, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingFastIntervalMin1, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingFastIntervalMax1, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), NopConnectCallback,
                              AdvertisingInterval::FAST2, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingFastIntervalMin2, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingFastIntervalMax2, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), NopConnectCallback,
                              AdvertisingInterval::SLOW, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingSlowIntervalMin, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingSlowIntervalMax, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));
}

TEST_F(LowEnergyAdvertisingManagerTest, NonConnectableAdvertisingIntervals) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp = CreateFakeAdvertisingData(21 /* size of ad */);

  // We expect FAST1 to fall back to FAST2 due to specification recommendation (Vol 3, Part C,
  // Appendix A) and lack of support for non-connectable advertising with FAST1 parameters on
  // certain controllers.
  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), nullptr,
                              AdvertisingInterval::FAST1, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingFastIntervalMin2, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingFastIntervalMax2, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), nullptr,
                              AdvertisingInterval::FAST2, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingFastIntervalMin2, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingFastIntervalMax2, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));

  adv_mgr()->StartAdvertising(CreateFakeAdvertisingData(),
                              CreateFakeAdvertisingData(21 /* size of ad */), nullptr,
                              AdvertisingInterval::SLOW, false /* anonymous */,
                              /*include_tx_power_level*/ false, GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());
  ASSERT_TRUE(current_adv());
  EXPECT_EQ(kLEAdvertisingSlowIntervalMin, current_adv()->interval_min);
  EXPECT_EQ(kLEAdvertisingSlowIntervalMax, current_adv()->interval_max);
  ASSERT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));
}

TEST_F(LowEnergyAdvertisingManagerTest, DestroyingInstanceStopsAdvertisement) {
  {
    AdvertisementInstance instance;
    adv_mgr()->StartAdvertising(AdvertisingData(), AdvertisingData(), nullptr,
                                AdvertisingInterval::FAST1, /*anonymous=*/false,
                                /*include_tx_power_level*/ false,
                                [&](AdvertisementInstance i, auto status) {
                                  ASSERT_TRUE(status.is_ok());
                                  instance = std::move(i);
                                });
    RunLoopUntilIdle();
    EXPECT_TRUE(adv_mgr()->advertising());

    // Destroying |instance| should stop the advertisement.
  }

  RunLoopUntilIdle();
  EXPECT_FALSE(adv_mgr()->advertising());
}

TEST_F(LowEnergyAdvertisingManagerTest, MovingIntoInstanceStopsAdvertisement) {
  AdvertisementInstance instance;
  adv_mgr()->StartAdvertising(AdvertisingData(), AdvertisingData(), nullptr,
                              AdvertisingInterval::FAST1, /*anonymous=*/false,
                              /*include_tx_power_level*/ false,
                              [&](AdvertisementInstance i, auto status) {
                                ASSERT_TRUE(status.is_ok());
                                instance = std::move(i);
                              });
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_mgr()->advertising());

  // Destroying |instance| by invoking the move assignment operator should stop the advertisement.
  instance = {};
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_mgr()->advertising());
}

TEST_F(LowEnergyAdvertisingManagerTest, MovingInstanceTransfersOwnershipOfAdvertisement) {
  auto instance = std::make_unique<AdvertisementInstance>();
  adv_mgr()->StartAdvertising(AdvertisingData(), AdvertisingData(), nullptr,
                              AdvertisingInterval::FAST1, /*anonymous=*/false,
                              /*include_tx_power_level*/ false,
                              [&](AdvertisementInstance i, auto status) {
                                ASSERT_TRUE(status.is_ok());
                                *instance = std::move(i);
                              });
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_mgr()->advertising());

  // Moving |instance| should transfer the ownership of the advertisement (assignment).
  {
    AdvertisementInstance move_assigned_instance = std::move(*instance);

    // Explicitly clearing the old instance should have no effect.
    *instance = {};
    RunLoopUntilIdle();
    EXPECT_TRUE(adv_mgr()->advertising());

    *instance = std::move(move_assigned_instance);
  }

  // Advertisement should not stop when |move_assigned_instance| goes out of scope as it no longer
  // owns the advertisement.
  RunLoopUntilIdle();
  EXPECT_TRUE(adv_mgr()->advertising());

  // Moving |instance| should transfer the ownership of the advertisement (move-constructor).
  {
    AdvertisementInstance move_constructed_instance(std::move(*instance));

    // Explicitly destroying the old instance should have no effect.
    instance.reset();
    RunLoopUntilIdle();
    EXPECT_TRUE(adv_mgr()->advertising());
  }

  // Advertisement should stop when |move_constructed_instance| goes out of scope.
  RunLoopUntilIdle();
  EXPECT_FALSE(adv_mgr()->advertising());
}

}  // namespace
}  // namespace gap
}  // namespace bt
