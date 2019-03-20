// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"

#include <map>

#include <zircon/assert.h>
#include <zircon/syscalls.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"

#include "lib/fxl/macros.h"
#include "lib/gtest/test_loop_fixture.h"

namespace bt {
namespace gap {
namespace {

using common::DeviceAddress;

constexpr size_t kDefaultMaxAds = 1;
constexpr size_t kDefaultMaxAdSize = 23;
constexpr size_t kDefaultFakeAdSize = 20;
constexpr zx::duration kTestInterval = zx::sec(1);

const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom,
                                   "00:11:22:33:44:55");

struct AdvertisementStatus {
  AdvertisingData data;
  AdvertisingData scan_rsp;
  bool anonymous;
  zx::duration interval;
  hci::LowEnergyAdvertiser::ConnectionCallback connect_cb;
};

// LowEnergyAdvertiser for testing purposes:
//  - Reports max_ads supported
//  - Reports mas_ad_size supported
//  - Actually just accepts all ads and stores them in ad_store
class FakeLowEnergyAdvertiser final : public hci::LowEnergyAdvertiser {
 public:
  FakeLowEnergyAdvertiser(
      size_t max_ads, size_t max_ad_size,
      std::map<common::DeviceAddress, AdvertisementStatus>* ad_store)
      : max_ads_(max_ads), max_ad_size_(max_ad_size), ads_(ad_store) {
    ZX_ASSERT(ads_);
  }

  ~FakeLowEnergyAdvertiser() override = default;

  size_t GetSizeLimit() override { return max_ad_size_; }

  size_t GetMaxAdvertisements() const override { return max_ads_; }

  bool AllowsRandomAddressChange() const override { return true; }

  void StartAdvertising(const common::DeviceAddress& address,
                        const common::ByteBuffer& data,
                        const common::ByteBuffer& scan_rsp,
                        ConnectionCallback connect_callback,
                        zx::duration interval, bool anonymous,
                        AdvertisingStatusCallback callback) override {
    if (!pending_error_) {
      callback(zx::duration(), pending_error_);
      pending_error_ = hci::Status();
      return;
    }
    if (data.size() > max_ad_size_) {
      callback(zx::duration(),
               hci::Status(common::HostError::kInvalidParameters));
      return;
    }
    if (scan_rsp.size() > max_ad_size_) {
      callback(zx::duration(),
               hci::Status(common::HostError::kInvalidParameters));
      return;
    }
    AdvertisementStatus new_status;
    AdvertisingData::FromBytes(data, &new_status.data);
    AdvertisingData::FromBytes(scan_rsp, &new_status.scan_rsp);
    new_status.connect_cb = std::move(connect_callback);
    new_status.interval = interval;
    new_status.anonymous = anonymous;
    ads_->emplace(address, std::move(new_status));
    callback(interval, hci::Status());
  }

  bool StopAdvertising(const common::DeviceAddress& address) override {
    ads_->erase(address);
    return true;
  }

  void OnIncomingConnection(
      hci::ConnectionHandle handle, hci::Connection::Role role,
      const common::DeviceAddress& peer_address,
      const hci::LEConnectionParameters& conn_params) override {
    // Right now, we call the first callback, because we can't call any other
    // ones.
    // TODO(jamuraa): make this send it to the correct callback once we can
    // determine which one that is.
    const auto& cb = ads_->begin()->second.connect_cb;
    if (cb) {
      cb(std::make_unique<hci::testing::FakeConnection>(
          handle, hci::Connection::LinkType::kLE, role, ads_->begin()->first,
          peer_address));
    }
  }

  // Sets this faker up to send an error back from the next StartAdvertising
  // call. Set to success to disable a previously called error.
  void ErrorOnNext(hci::Status error_status) { pending_error_ = error_status; }

 private:
  size_t max_ads_, max_ad_size_;
  std::map<common::DeviceAddress, AdvertisementStatus>* ads_;
  hci::Status pending_error_;

  FXL_DISALLOW_COPY_AND_ASSIGN(FakeLowEnergyAdvertiser);
};

using TestingBase = ::gtest::TestLoopFixture;

class GAP_LowEnergyAdvertisingManagerTest : public TestingBase,
                                            public hci::LocalAddressDelegate {
 public:
  GAP_LowEnergyAdvertisingManagerTest() = default;
  ~GAP_LowEnergyAdvertisingManagerTest() override = default;

 protected:
  void SetUp() override {
    set_local_address(kRandomAddress);
    MakeFakeAdvertiser();
    MakeAdvertisingManager();
  }

  void TearDown() override {
    adv_mgr_ = nullptr;
    advertiser_ = nullptr;
  }

  // Makes some fake advertising data of a specific |packed_size|
  AdvertisingData CreateFakeAdvertisingData(
      size_t packed_size = kDefaultFakeAdSize) {
    AdvertisingData result;
    auto buffer = common::CreateStaticByteBuffer(0x00, 0x01, 0x02, 0x03, 0x04,
                                                 0x05, 0x06, 0x07, 0x08);
    size_t bytes_left = packed_size;
    while (bytes_left > 0) {
      // Each field to take 10 bytes total, unless the next header (4 bytes)
      // won't fit. In which case we add enough bytes to finish up.
      size_t data_bytes = bytes_left < 14 ? (bytes_left - 4) : 6;
      result.SetManufacturerData(0xb000 + bytes_left,
                                 buffer.view(0, data_bytes));
      bytes_left = packed_size - result.CalculateBlockSize();
    }
    return result;
  }

  LowEnergyAdvertisingManager::AdvertisingStatusCallback GetErrorCallback() {
    return [this](AdvertisementId ad_id, hci::Status status) {
      EXPECT_EQ(kInvalidAdvertisementId, ad_id);
      EXPECT_FALSE(status);
      last_status_ = status;
    };
  }

  LowEnergyAdvertisingManager::AdvertisingStatusCallback GetSuccessCallback() {
    return [this](AdvertisementId ad_id, hci::Status status) {
      last_ad_id_ = ad_id;
      EXPECT_NE(kInvalidAdvertisementId, ad_id);
      EXPECT_TRUE(status);
      last_status_ = status;
    };
  }

  void MakeFakeAdvertiser(size_t max_ads = kDefaultMaxAds,
                          size_t max_ad_size = kDefaultMaxAdSize) {
    advertiser_ = std::make_unique<FakeLowEnergyAdvertiser>(
        max_ads, max_ad_size, &ad_store_);
  }

  void MakeAdvertisingManager() {
    adv_mgr_ =
        std::make_unique<LowEnergyAdvertisingManager>(advertiser(), this);
  }

  // LocalAddressDelegate override:
  void EnsureLocalAddress(AddressCallback callback) override {
    callback(local_address_);
  }

  LowEnergyAdvertisingManager* adv_mgr() const { return adv_mgr_.get(); }
  const std::map<common::DeviceAddress, AdvertisementStatus>& ad_store() {
    return ad_store_;
  }
  AdvertisementId last_ad_id() const { return last_ad_id_; }

  // Returns and clears the last callback status. This resets the state to
  // detect another callback.
  const std::optional<hci::Status> MoveLastStatus() {
    return std::move(last_status_);
  }

  FakeLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }
  void set_local_address(const DeviceAddress& local_address) {
    local_address_ = local_address;
  }

 private:
  std::map<common::DeviceAddress, AdvertisementStatus> ad_store_;
  AdvertisementId last_ad_id_;
  std::optional<hci::Status> last_status_;
  DeviceAddress local_address_;
  std::unique_ptr<FakeLowEnergyAdvertiser> advertiser_;
  std::unique_ptr<LowEnergyAdvertisingManager> adv_mgr_;

  FXL_DISALLOW_COPY_AND_ASSIGN(GAP_LowEnergyAdvertisingManagerTest);
};

// Tests:
//  - When the advertiser succeeds, the callback is called with the success
TEST_F(GAP_LowEnergyAdvertisingManagerTest, Success) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;  // Empty scan response

  set_local_address(kRandomAddress);
  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, kTestInterval,
                              false /* anonymous */, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  ASSERT_EQ(1u, ad_store().size());

  // Verify that the advertiser uses the requested local address.
  EXPECT_EQ(kRandomAddress, ad_store().begin()->first);
}

TEST_F(GAP_LowEnergyAdvertisingManagerTest, DataSize) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;  // Empty scan response

  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, kTestInterval,
                              false /* anonymous */, GetSuccessCallback());

  RunLoopUntilIdle();

  fake_ad = CreateFakeAdvertisingData(kDefaultMaxAdSize + 1);

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());

  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, kTestInterval,
                              false /* anonymous */, GetErrorCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());
}

// TODO(BT-742): Revise this test to use multiple advertising instances when
// multi-advertising is supported.
//  - Stopping one that is registered stops it in the advertiser
//    (and stops the right address)
//  - Stopping an advertisement that isn't registered retuns false
TEST_F(GAP_LowEnergyAdvertisingManagerTest, RegisterUnregister) {
  MakeFakeAdvertiser(2 /* ads available */);
  MakeAdvertisingManager();

  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;  // Empty scan response

  EXPECT_FALSE(adv_mgr()->StopAdvertising(kInvalidAdvertisementId));

  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, kTestInterval,
                              false /* anonymous */, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());

  EXPECT_TRUE(adv_mgr()->StopAdvertising(last_ad_id()));
  EXPECT_TRUE(ad_store().empty());

  EXPECT_FALSE(adv_mgr()->StopAdvertising(last_ad_id()));
  EXPECT_TRUE(ad_store().empty());
}

//  - When the advertiser returns an error, we return an error
TEST_F(GAP_LowEnergyAdvertisingManagerTest, AdvertiserError) {
  advertiser()->ErrorOnNext(hci::Status(hci::kInvalidHCICommandParameters));
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;

  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, kTestInterval,
                              false /* anonymous */, GetErrorCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
}

//  - It calls the connectable callback correctly when connected to
TEST_F(GAP_LowEnergyAdvertisingManagerTest, ConnectCallback) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;

  hci::ConnectionPtr link;
  AdvertisementId advertised_id = kInvalidAdvertisementId;

  auto connect_cb = [&](AdvertisementId connected_id,
                        hci::ConnectionPtr cb_link) {
    link = std::move(cb_link);
    EXPECT_EQ(advertised_id, connected_id);
  };
  set_local_address(kRandomAddress);
  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, connect_cb, kTestInterval,
                              false /* anonymous */, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  advertised_id = last_ad_id();

  DeviceAddress peer_address(DeviceAddress::Type::kLEPublic,
                             "03:02:01:01:02:03");
  advertiser()->OnIncomingConnection(1, hci::Connection::Role::kSlave,
                                     peer_address,
                                     hci::LEConnectionParameters());
  RunLoopUntilIdle();
  ASSERT_TRUE(link);

  // Make sure that the link has the correct local and peer addresses assigned.
  EXPECT_EQ(kRandomAddress, link->local_address());
  EXPECT_EQ(peer_address, link->peer_address());
}

//  - Error: Connectable and Anonymous at the same time
TEST_F(GAP_LowEnergyAdvertisingManagerTest, ConnectAdvertiseError) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp;

  auto connect_cb = [this](AdvertisementId connected_id,
                           hci::ConnectionPtr conn) {};

  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, connect_cb, kTestInterval,
                              true /* anonymous */, GetErrorCallback());

  EXPECT_TRUE(MoveLastStatus());
}

//  - Passes the values for the data on. (anonymous, data, scan_rsp,
//  interval_ms)
TEST_F(GAP_LowEnergyAdvertisingManagerTest, SendsCorrectData) {
  AdvertisingData fake_ad = CreateFakeAdvertisingData();
  AdvertisingData scan_rsp = CreateFakeAdvertisingData(21 /* size of ad */);

  zx_duration_t random = 0;
  zx_cprng_draw(&random, sizeof(random));
  auto interval = zx::duration(random);
  adv_mgr()->StartAdvertising(fake_ad, scan_rsp, nullptr, interval,
                              false /* anonymous */, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_EQ(1u, ad_store().size());

  auto ad_status = &ad_store().begin()->second;

  EXPECT_EQ(fake_ad, ad_status->data);
  EXPECT_EQ(scan_rsp, ad_status->scan_rsp);
  EXPECT_EQ(false, ad_status->anonymous);
  EXPECT_EQ(nullptr, ad_status->connect_cb);
  EXPECT_EQ(interval, ad_status->interval);
}

}  // namespace
}  // namespace gap
}  // namespace bt
