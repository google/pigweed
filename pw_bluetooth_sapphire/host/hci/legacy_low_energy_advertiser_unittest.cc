// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_device.h"
#include "lib/fxl/macros.h"

namespace bt {

using common::BufferView;
using testing::FakeController;

namespace hci {
namespace {

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

constexpr ConnectionHandle kHandle = 0x0001;

const common::DeviceAddress kPublicAddress(
    common::DeviceAddress::Type::kLEPublic, "00:00:00:00:00:01");
const common::DeviceAddress kRandomAddress(
    common::DeviceAddress::Type::kLERandom, "00:00:00:00:00:02");

constexpr size_t kDefaultAdSize = 20;
constexpr zx::duration kTestInterval = zx::sec(1);

void NopConnectionCallback(ConnectionPtr) {}

class HCI_LegacyLowEnergyAdvertiserTest : public TestingBase {
 public:
  HCI_LegacyLowEnergyAdvertiserTest() = default;
  ~HCI_LegacyLowEnergyAdvertiserTest() override = default;

 protected:
  // TestingBase overrides:
  void SetUp() override {
    TestingBase::SetUp();

    // ACL data channel needs to be present for production hci::Connection
    // objects.
    TestingBase::InitializeACLDataChannel(
        hci::DataBufferInfo(),
        hci::DataBufferInfo(hci::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    settings.bd_addr = kPublicAddress;
    test_device()->set_settings(settings);

    advertiser_ = std::make_unique<LegacyLowEnergyAdvertiser>(transport());

    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    advertiser_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  LegacyLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

  LowEnergyAdvertiser::AdvertisingStatusCallback GetSuccessCallback() {
    return [this](zx::duration interval, Status status) {
      last_status_ = status;
      EXPECT_TRUE(status) << status.ToString();
    };
  }

  LowEnergyAdvertiser::AdvertisingStatusCallback GetErrorCallback() {
    return [this](zx::duration interval, Status status) {
      last_status_ = status;
      EXPECT_FALSE(status);
    };
  }

  // Retrieves the last status, and resets the last status to empty.
  std::optional<Status> MoveLastStatus() { return std::move(last_status_); }

  // Makes some fake advertising data of a specific |packed_size|
  common::DynamicByteBuffer GetExampleData(size_t size = kDefaultAdSize) {
    common::DynamicByteBuffer result(size);
    // Count backwards.
    for (size_t i = 0; i < size; i++) {
      result[i] = (uint8_t)((size - i) % 255);
    }
    return result;
  }

 private:
  std::unique_ptr<LegacyLowEnergyAdvertiser> advertiser_;

  std::optional<Status> last_status_;

  FXL_DISALLOW_COPY_AND_ASSIGN(HCI_LegacyLowEnergyAdvertiserTest);
};

// TODO(jamuraa): Use typed tests to test LowEnergyAdvertiser common properties

// - Error when the advertisement data is too large
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertisementSizeTest) {
  // 4 bytes long (adv length: 7 bytes)
  auto reasonable_data = common::CreateStaticByteBuffer(0x20, 0x06, 0xaa, 0xfe,
                                                        'T', 'e', 's', 't');
  // 30 bytes long (adv length: 33 bytes)
  auto oversize_data = common::CreateStaticByteBuffer(
      0x20, 0x20, 0xaa, 0xfe, 'T', 'h', 'e', 'q', 'u', 'i', 'c', 'k', 'b', 'r',
      'o', 'w', 'n', 'f', 'o', 'x', 'w', 'a', 'g', 'g', 'e', 'd', 'i', 't', 's',
      't', 'a', 'i', 'l', '.');

  common::DynamicByteBuffer scan_data;

  // Should accept ads that are of reasonable size
  advertiser()->StartAdvertising(kPublicAddress, reasonable_data, scan_data,
                                 nullptr, kTestInterval, false,
                                 GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());

  advertiser()->StopAdvertising(kPublicAddress);

  // And reject ads that are too big
  advertiser()->StartAdvertising(kPublicAddress, oversize_data, scan_data,
                                 nullptr, kTestInterval, false,
                                 GetErrorCallback());
  EXPECT_TRUE(MoveLastStatus());
}

// - Stops the advertisement when an incoming connection comes
// - Calls the connection callback correctly when it's setup
// - Checks that advertising state is cleaned up.
// - Checks that it is possible to restart advertising.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, ConnectionTest) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  ConnectionPtr link;
  auto conn_cb = [&link](auto cb_link) { link = std::move(cb_link); };

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, conn_cb,
                                 kTestInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  // The connection manager will hand us a connection when one gets created.
  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave,
                                     kRandomAddress, LEConnectionParameters());
  ASSERT_TRUE(link);
  EXPECT_EQ(kHandle, link->handle());
  EXPECT_EQ(kPublicAddress, link->local_address());
  EXPECT_EQ(kRandomAddress, link->peer_address());
  link->set_closed();

  // Advertising state should get cleared.
  RunLoopUntilIdle();

  // StopAdvertising() sends multiple HCI commands. We only check that the
  // first one succeeded. StartAdvertising cancels the rest of the sequence
  // below.
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // Restart advertising using kRandomAddress.
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, conn_cb,
                                 kTestInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  // Accept a connection from kPublicAddress. The local and peer addresses
  // should get assigned correctly.
  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave,
                                     kPublicAddress, LEConnectionParameters());
  ASSERT_TRUE(link);
  EXPECT_EQ(kRandomAddress, link->local_address());
  EXPECT_EQ(kPublicAddress, link->peer_address());
  link->set_closed();
}

// Tests that advertising can be restarted right away in a connection callback.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, RestartInConnectionCallback) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  ConnectionPtr link;
  auto conn_cb = [&, this](auto cb_link) {
    link = std::move(cb_link);
    advertiser()->StartAdvertising(kPublicAddress, ad, scan_data,
                                   NopConnectionCallback, kTestInterval, false,
                                   GetSuccessCallback());
  };

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, conn_cb,
                                 kTestInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  bool enabled = true;
  std::vector<bool> adv_states;
  test_device()->SetAdvertisingStateCallback(
      [this, &adv_states, &enabled] {
        bool new_enabled = test_device()->le_advertising_state().enabled;
        if (enabled != new_enabled) {
          adv_states.push_back(new_enabled);
          enabled = new_enabled;
        }
      },
      dispatcher());

  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave,
                                     kRandomAddress, LEConnectionParameters());

  // Advertising should get disabled and re-enabled.
  RunLoopUntilIdle();
  ASSERT_EQ(2u, adv_states.size());
  EXPECT_FALSE(adv_states[0]);
  EXPECT_TRUE(adv_states[1]);
}

// Tests starting and stopping an advertisement.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartAndStop) {
  constexpr zx::duration kInterval = zx::msec(500);
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
}

// Tests that an advertisement is configured with the correct parameters.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertisingParameters) {
  constexpr zx::duration kInterval = zx::msec(500);
  constexpr uint16_t kIntervalSlices = 800;
  auto ad = GetExampleData();
  BufferView scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  // Verify the fake controller state.
  const auto& fake_adv_state = test_device()->le_advertising_state();
  EXPECT_TRUE(fake_adv_state.enabled);
  EXPECT_EQ(kIntervalSlices, fake_adv_state.interval);
  EXPECT_EQ(fake_adv_state.advertised_view(), ad);
  EXPECT_EQ(0u, fake_adv_state.scan_rsp_view().size());
  EXPECT_EQ(hci::LEOwnAddressType::kRandom, fake_adv_state.own_address_type);

  // Restart advertising with a public address and verify that the configured
  // local address type is correct.
  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, nullptr,
                                 kInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(fake_adv_state.enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic, fake_adv_state.own_address_type);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStarting) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;
  common::DeviceAddress addr = kRandomAddress;

  advertiser()->StartAdvertising(addr, ad, scan_data, nullptr, kTestInterval,
                                 false, [](auto, auto) {});
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  advertiser()->StartAdvertising(addr, ad, scan_data, nullptr, kTestInterval,
                                 false, GetErrorCallback());
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  auto status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(common::HostError::kInProgress, status->error());
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStopping) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;
  common::DeviceAddress addr = kRandomAddress;

  // Get to a started state.
  advertiser()->StartAdvertising(addr, ad, scan_data, nullptr, kTestInterval,
                                 false, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  // Initiate a request to Stop and wait until it's partially in progress.
  bool enabled = true;
  bool was_disabled = false;
  auto adv_state_cb = [&] {
    enabled = test_device()->le_advertising_state().enabled;
    if (!was_disabled && !enabled) {
      was_disabled = true;

      // Starting now should cancel the stop sequence and succeed.
      advertiser()->StartAdvertising(addr, ad, scan_data, nullptr,
                                     kTestInterval, false,
                                     GetSuccessCallback());
    }
  };
  test_device()->SetAdvertisingStateCallback(adv_state_cb, dispatcher());

  EXPECT_TRUE(advertiser()->StopAdvertising(addr));

  // Advertising should have been momentarily disabled.
  RunLoopUntilIdle();
  EXPECT_TRUE(was_disabled);
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
}

// - StopAdvertisement noops when the advertisement address is wrong
// - Sets the advertisement data to null when stopped to prevent data leakage
//   (re-enable advertising without changing data, intercept)
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StopAdvertisingConditions) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kTestInterval, false, GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());

  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));
  EXPECT_FALSE(advertiser()->StopAdvertising(kPublicAddress));

  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));

  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));

  RunLoopUntilIdle();

  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(0u, test_device()->le_advertising_state().advertised_view().size());
  EXPECT_EQ(0u, test_device()->le_advertising_state().scan_rsp_view().size());
}

// - Rejects StartAdvertising for a different address when Advertising already
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, NoAdvertiseTwice) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kTestInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_advertising_state().own_address_type);

  uint8_t before = ad[0];
  ad[0] = 0xff;
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, nullptr,
                                 kTestInterval, false, GetErrorCallback());
  ad[0] = before;
  RunLoopUntilIdle();

  // Should still be using the random address.
  EXPECT_EQ(hci::LEOwnAddressType::kRandom,
            test_device()->le_advertising_state().own_address_type);
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));
}

// - Updates data and params for the same address when advertising already
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertiseUpdate) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kTestInterval, false, GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));

  ad[0] = 0xff;
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 zx::msec(2500), false, GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(ContainersEqual(
      test_device()->le_advertising_state().advertised_view(), ad));
  // 2500 ms = 4000 timeslices
  EXPECT_EQ(4000, test_device()->le_advertising_state().interval);
}

// - Rejects anonymous advertisement (unsupported)
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, NoAnonymous) {
  common::DynamicByteBuffer ad = GetExampleData();
  common::DynamicByteBuffer scan_data;

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, nullptr,
                                 kTestInterval, true, GetErrorCallback());
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AllowsRandomAddressChange) {
  // The random address can be changed while not advertising.
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());

  // The random address cannot be changed while starting to advertise.
  advertiser()->StartAdvertising(kRandomAddress, GetExampleData(), BufferView(),
                                 nullptr, zx::sec(1), false,
                                 GetSuccessCallback());
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  EXPECT_FALSE(advertiser()->AllowsRandomAddressChange());

  // The random address cannot be changed while advertising is enabled.
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_FALSE(advertiser()->AllowsRandomAddressChange());

  // The advertiser allows changing the address while advertising is getting
  // stopped.
  advertiser()->StopAdvertising(kRandomAddress);
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());

  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());
}

}  // namespace
}  // namespace hci
}  // namespace bt
