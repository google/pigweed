// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_advertiser.h"

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
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

constexpr ConnectionHandle kHandle = 0x0001;

const DeviceAddress kPublicAddress(DeviceAddress::Type::kLEPublic, {1});
const DeviceAddress kRandomAddress(DeviceAddress::Type::kLERandom, {2});

constexpr AdvertisingIntervalRange kTestInterval(kLEAdvertisingIntervalMin,
                                                 kLEAdvertisingIntervalMax);

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
    TestingBase::InitializeACLDataChannel(hci::DataBufferInfo(),
                                          hci::DataBufferInfo(hci::kMaxACLPayloadSize, 10));

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    settings.bd_addr = kPublicAddress;
    test_device()->set_settings(settings);

    advertiser_ = std::make_unique<LegacyLowEnergyAdvertiser>(transport()->WeakPtr());

    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
  }

  void TearDown() override {
    advertiser_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  LegacyLowEnergyAdvertiser* advertiser() const { return advertiser_.get(); }

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

  // Retrieves the last status, and resets the last status to empty.
  std::optional<Status> MoveLastStatus() { return std::move(last_status_); }

  // Makes some fake advertising data.
  // |include_flags| signals whether to include flag encoding size in the data calculation.
  AdvertisingData GetExampleData(bool include_flags = true) {
    AdvertisingData result;

    auto name = "fuchsia";
    EXPECT_TRUE(result.SetLocalName(name));

    auto appearance = 0x1234;
    result.SetAppearance(appearance);

    EXPECT_LE(result.CalculateBlockSize(include_flags), kMaxLEAdvertisingDataLength);

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

    // The maximum advertisement packet is: |kMaxLEAdvertisingDataLength| = 31, and |result| = 32
    // bytes. |result| should be too large to advertise.
    EXPECT_GT(result.CalculateBlockSize(include_flags), kMaxLEAdvertisingDataLength);

    return result;
  }

 private:
  std::unique_ptr<LegacyLowEnergyAdvertiser> advertiser_;

  std::optional<Status> last_status_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HCI_LegacyLowEnergyAdvertiserTest);
};

// TODO(jamuraa): Use typed tests to test LowEnergyAdvertiser common properties

// - Stops the advertisement when an incoming connection comes
// - Calls the connection callback correctly when it's setup
// - Checks that advertising state is cleaned up.
// - Checks that it is possible to restart advertising.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, ConnectionTest) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, /*anonymous=*/false, kDefaultNoAdvFlags, false);

  ConnectionPtr link;
  auto conn_cb = [&link](auto cb_link) { link = std::move(cb_link); };

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, conn_cb,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  // The connection manager will hand us a connection when one gets created.
  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave, kRandomAddress,
                                     LEConnectionParameters());
  ASSERT_TRUE(link);
  EXPECT_EQ(kHandle, link->handle());
  EXPECT_EQ(kPublicAddress, link->local_address());
  EXPECT_EQ(kRandomAddress, link->peer_address());
  link->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
  test_device()->SendDisconnectionCompleteEvent(link->handle());

  // Advertising state should get cleared.
  RunLoopUntilIdle();

  // StopAdvertising() sends multiple HCI commands. We only check that the
  // first one succeeded. StartAdvertising cancels the rest of the sequence
  // below.
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // Restart advertising using kRandomAddress.
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, conn_cb,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  // Accept a connection from kPublicAddress. The local and peer addresses
  // should get assigned correctly.
  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave, kPublicAddress,
                                     LEConnectionParameters());
  // ASSERT_TRUE(link);
  EXPECT_EQ(kRandomAddress, link->local_address());
  EXPECT_EQ(kPublicAddress, link->peer_address());
  link->Disconnect(StatusCode::kRemoteUserTerminatedConnection);
}

// Tests that advertising can be restarted right away in a connection callback.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, RestartInConnectionCallback) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  ConnectionPtr link;
  auto conn_cb = [&, this](auto cb_link) {
    link = std::move(cb_link);
    advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, NopConnectionCallback,
                                   GetSuccessCallback());
  };

  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, conn_cb,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  bool enabled = true;
  std::vector<bool> adv_states;
  test_device()->set_advertising_state_callback([this, &adv_states, &enabled] {
    bool new_enabled = test_device()->le_advertising_state().enabled;
    if (enabled != new_enabled) {
      adv_states.push_back(new_enabled);
      enabled = new_enabled;
    }
  });

  advertiser()->OnIncomingConnection(kHandle, Connection::Role::kSlave, kRandomAddress,
                                     LEConnectionParameters());

  // Advertising should get disabled and re-enabled.
  RunLoopUntilIdle();
  ASSERT_EQ(2u, adv_states.size());
  EXPECT_FALSE(adv_states[0]);
  EXPECT_TRUE(adv_states[1]);
}

// An incoming connection when not advertising should get disconnected.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, IncomingConnectionWhenNotAdvertising) {
  std::vector<std::pair<bool, ConnectionHandle>> connection_states;
  test_device()->set_connection_state_callback(
      [&](const auto& address, auto handle, bool connected, bool canceled) {
        EXPECT_EQ(kRandomAddress, address);
        EXPECT_FALSE(canceled);
        connection_states.push_back(std::make_pair(connected, handle));
      });

  auto fake_peer = std::make_unique<FakePeer>(kRandomAddress, true, true);
  test_device()->AddPeer(std::move(fake_peer));
  test_device()->ConnectLowEnergy(kRandomAddress, ConnectionRole::kSlave);
  RunLoopUntilIdle();

  ASSERT_EQ(1u, connection_states.size());
  auto [connection_state, handle] = connection_states[0];
  EXPECT_TRUE(connection_state);

  // Notify the advertiser of the incoming connection. It should reject it and the controller should
  // become disconnected.
  advertiser()->OnIncomingConnection(handle, Connection::Role::kSlave, kRandomAddress,
                                     LEConnectionParameters());
  RunLoopUntilIdle();
  ASSERT_EQ(2u, connection_states.size());
  auto [connection_state_after_disconnect, disconnected_handle] = connection_states[1];
  EXPECT_EQ(handle, disconnected_handle);
  EXPECT_FALSE(connection_state_after_disconnect);
}

// An incoming connection during non-connectable advertising should get disconnected.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, IncomingConnectionWhenNonConnectableAdvertising) {
  AdvertisingData empty;
  AdvertisingData scan_rsp_empty;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kPublicAddress, empty, scan_rsp_empty, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  ASSERT_TRUE(MoveLastStatus());

  std::vector<std::pair<bool, ConnectionHandle>> connection_states;
  test_device()->set_connection_state_callback(
      [&](const auto& address, auto handle, bool connected, bool canceled) {
        EXPECT_EQ(kRandomAddress, address);
        EXPECT_FALSE(canceled);
        connection_states.push_back(std::make_pair(connected, handle));
      });

  auto fake_peer = std::make_unique<FakePeer>(kRandomAddress, true, true);
  test_device()->AddPeer(std::move(fake_peer));
  test_device()->ConnectLowEnergy(kRandomAddress, ConnectionRole::kSlave);
  RunLoopUntilIdle();

  ASSERT_EQ(1u, connection_states.size());
  auto [connection_state, handle] = connection_states[0];
  EXPECT_TRUE(connection_state);

  // Notify the advertiser of the incoming connection. It should reject it and the controller should
  // become disconnected.
  advertiser()->OnIncomingConnection(handle, Connection::Role::kSlave, kRandomAddress,
                                     LEConnectionParameters());
  RunLoopUntilIdle();
  ASSERT_EQ(2u, connection_states.size());
  auto [connection_state_after_disconnect, disconnected_handle] = connection_states[1];
  EXPECT_EQ(handle, disconnected_handle);
  EXPECT_FALSE(connection_state_after_disconnect);
}

// Tests starting and stopping an advertisement.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartAndStop) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
}

// Tests that an advertisement is configured with the correct parameters.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertisingParameters) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  auto flags = AdvFlag::kLEGeneralDiscoverableMode;
  AdvertisingOptions options(kTestInterval, false, flags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  // The expected advertisement including the Flags.
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags*/ true));
  ad.WriteBlock(&expected_ad, flags);

  // Verify the fake controller state.
  const auto& fake_adv_state = test_device()->le_advertising_state();
  EXPECT_TRUE(fake_adv_state.enabled);
  EXPECT_EQ(kTestInterval.min(), fake_adv_state.interval_min);
  EXPECT_EQ(kTestInterval.max(), fake_adv_state.interval_max);
  EXPECT_EQ(expected_ad, fake_adv_state.advertised_view());
  EXPECT_EQ(0u, fake_adv_state.scan_rsp_view().size());
  EXPECT_EQ(hci::LEOwnAddressType::kRandom, fake_adv_state.own_address_type);

  // Restart advertising with a public address and verify that the configured
  // local address type is correct.
  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));
  AdvertisingOptions new_options(kTestInterval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, new_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(fake_adv_state.enabled);
  EXPECT_EQ(hci::LEOwnAddressType::kPublic, fake_adv_state.own_address_type);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertisingDataTooLong) {
  AdvertisingData invalid_ad =
      GetTooLargeExampleData(/*include_tx_power=*/false, /*include_flags=*/true);
  AdvertisingData valid_scan_rsp = GetExampleData(/*include_flags=*/false);
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  // Advertising data too large.
  advertiser()->StartAdvertising(kRandomAddress, invalid_ad, valid_scan_rsp, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();
  auto status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(HostError::kAdvertisingDataTooLong, status->error());

  // Check with TX Power included.
  invalid_ad = GetTooLargeExampleData(/*include_tx_power=*/true, /*include_flags=*/true);
  advertiser()->StartAdvertising(kRandomAddress, invalid_ad, valid_scan_rsp, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();
  status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(HostError::kAdvertisingDataTooLong, status->error());
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, ScanResponseTooLong) {
  AdvertisingData valid_ad = GetExampleData();
  AdvertisingData invalid_scan_rsp =
      GetTooLargeExampleData(/*include_tx_power=*/false, /*include_flags=*/false);
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kRandomAddress, valid_ad, invalid_scan_rsp, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();
  auto status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(HostError::kScanResponseTooLong, status->error());

  // Check with TX Power included.
  invalid_scan_rsp = GetTooLargeExampleData(/*include_tx_power=*/true, /*include_flags=*/false);
  advertiser()->StartAdvertising(kRandomAddress, valid_ad, invalid_scan_rsp, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();
  status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(HostError::kScanResponseTooLong, status->error());
}

// Tests that advertising interval values are capped within the allowed range.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertisingIntervalWithinAllowedRange) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;

  // Pass min and max values that are outside the allowed range. These should be capped.
  constexpr AdvertisingIntervalRange interval(0x0000, 0xFFFF);
  AdvertisingOptions options(interval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  const auto& fake_adv_state = test_device()->le_advertising_state();
  EXPECT_EQ(kLEAdvertisingIntervalMin, fake_adv_state.interval_min);
  EXPECT_EQ(kLEAdvertisingIntervalMax, fake_adv_state.interval_max);

  // Reconfigure with values that are within the range. These should get passed down as is.
  const AdvertisingIntervalRange new_interval(kLEAdvertisingIntervalMin + 1,
                                              kLEAdvertisingIntervalMax - 1);
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, new_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());

  EXPECT_EQ(new_interval.min(), fake_adv_state.interval_min);
  EXPECT_EQ(new_interval.max(), fake_adv_state.interval_max);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStarting) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions old_options(old_interval, false, kDefaultNoAdvFlags, false);
  const AdvertisingIntervalRange new_interval(kTestInterval.min(), kTestInterval.min());
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(addr, ad, scan_data, old_options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStopping) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  // Get to a started state.
  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, GetSuccessCallback());
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
      advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, GetSuccessCallback());
    }
  };
  test_device()->set_advertising_state_callback(adv_state_cb);

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
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());

  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());

  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
  EXPECT_FALSE(advertiser()->StopAdvertising(kPublicAddress));

  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));

  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));

  RunLoopUntilIdle();

  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(0u, test_device()->le_advertising_state().advertised_view().size());
  EXPECT_EQ(0u, test_device()->le_advertising_state().scan_rsp_view().size());
}

// - Rejects StartAdvertising for a different address when Advertising already
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, NoAdvertiseTwice) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
  EXPECT_EQ(hci::LEOwnAddressType::kRandom, test_device()->le_advertising_state().own_address_type);

  uint16_t new_appearance = 0x6789;
  ad.SetAppearance(new_appearance);
  advertiser()->StartAdvertising(kPublicAddress, ad, scan_data, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();

  // Should still be using the random address.
  EXPECT_EQ(hci::LEOwnAddressType::kRandom, test_device()->le_advertising_state().own_address_type);
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
}

// - Updates data and params for the same address when advertising already
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AdvertiseUpdate) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  // The expected advertising data payload, with the flags.
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));

  EXPECT_EQ(kTestInterval.min(), test_device()->le_advertising_state().interval_min);
  EXPECT_EQ(kTestInterval.max(), test_device()->le_advertising_state().interval_max);

  uint16_t new_appearance = 0x6789;
  ad.SetAppearance(new_appearance);

  const AdvertisingIntervalRange new_interval(kTestInterval.min() + 1, kTestInterval.max() - 1);
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, false);
  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, new_options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();

  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  DynamicByteBuffer expected_new_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_new_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_new_ad));

  EXPECT_EQ(new_interval.min(), test_device()->le_advertising_state().interval_min);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);
}

// - Rejects anonymous advertisement (unsupported)
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, NoAnonymous) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, true, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetErrorCallback());
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
}

TEST_F(HCI_LegacyLowEnergyAdvertiserTest, AllowsRandomAddressChange) {
  AdvertisingData scan_rsp;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, false);

  // The random address can be changed while not advertising.
  EXPECT_TRUE(advertiser()->AllowsRandomAddressChange());

  // The random address cannot be changed while starting to advertise.
  advertiser()->StartAdvertising(kRandomAddress, GetExampleData(), scan_rsp, options, nullptr,
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

// Tests starting and stopping an advertisement when the TX power is requested.
// Validates the advertising and scan response data are correctly populated with the
// TX power.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartAndStopWithTxPower) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data = GetExampleData();
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, true);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);

  // Verify the advertising and scan response data contains the newly populated TX Power Level.
  // See |../testing/fake_controller.cc:1585| for return value.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));

  scan_data.SetTxPower(0x9);
  DynamicByteBuffer expected_scan_rsp(ad.CalculateBlockSize(/*include_flags=*/false));
  scan_data.WriteBlock(&expected_scan_rsp, std::nullopt);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().scan_rsp_view(), expected_scan_rsp));

  EXPECT_TRUE(advertiser()->StopAdvertising(kRandomAddress));
  RunLoopUntilIdle();
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);
}

// Tests sending a second StartAdvertising command while the first one is outstanding,
// with TX power enabled.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStartingWithTxPower) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, false, kDefaultNoAdvFlags, true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min(), kTestInterval.min());
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, true);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);

  // Verify the advertising data contains the newly populated TX Power Level.
  // Since the scan response data is empty, it's power level should not be populated.
  // See |../testing/fake_controller.cc:1585| for return value.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().scan_rsp_view(), DynamicByteBuffer()));
}

// Test that the second StartAdvertising call (with no TX Power requested) successfully supercedes
// the first ongoing StartAdvertising call (with TX Power requested).
// Validates the advertised data does not include the TX power.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileStartingTxPowerRequestedThenNotRequested) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, false, kDefaultNoAdvFlags, true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min(), kTestInterval.min());
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, false);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);

  // Verify the advertising data doesn't contain a new TX Power Level.
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
}

// Test that the second StartAdvertising call (with TX Power requested) successfully supercedes
// the first ongoing StartAdvertising call (no TX Power requested).
// Validates the advertised data includes the TX power.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartingWhileStartingTxPowerNotRequestedThenRequested) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, false, kDefaultNoAdvFlags, false);
  const AdvertisingIntervalRange new_interval(kTestInterval.min(), kTestInterval.min());
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, true);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, [](auto) {});
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  // This call should override the previous call and succeed with the new parameters.
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr, GetSuccessCallback());
  RunLoopUntilIdle();
  EXPECT_TRUE(MoveLastStatus());
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);

  // Verify the advertising data doesn't contain a new TX Power Level.
  ad.SetTxPower(0x9);
  DynamicByteBuffer expected_ad(ad.CalculateBlockSize(/*include_flags=*/true));
  ad.WriteBlock(&expected_ad, kDefaultNoAdvFlags);
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().advertised_view(), expected_ad));
  EXPECT_TRUE(
      ContainersEqual(test_device()->le_advertising_state().scan_rsp_view(), DynamicByteBuffer()));
}

// Tests that advertising gets enabled successfully with the updated parameters if
// StartAdvertising is called during a TX Power Level read.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartWhileTxPowerReadSuccess) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  DeviceAddress addr = kRandomAddress;

  const AdvertisingIntervalRange old_interval = kTestInterval;
  AdvertisingOptions options(old_interval, false, kDefaultNoAdvFlags, true);
  const AdvertisingIntervalRange new_interval(kTestInterval.min(), kTestInterval.min());
  AdvertisingOptions new_options(new_interval, false, kDefaultNoAdvFlags, true);

  // Hold off on responding to the first TX Power Level Read command.
  test_device()->set_tx_power_level_read_response_flag(/*respond=*/false);

  advertiser()->StartAdvertising(addr, ad, scan_data, options, nullptr, GetErrorCallback());
  EXPECT_FALSE(test_device()->le_advertising_state().enabled);

  RunLoopUntilIdle();
  // At this point in time, the first StartAdvertising call is still waiting on the TX Power Level
  // Read response.

  // Queue up the next StartAdvertising call.
  // This call should override the previous call's advertising parameters.
  test_device()->set_tx_power_level_read_response_flag(/*respond=*/true);
  advertiser()->StartAdvertising(addr, ad, scan_data, new_options, nullptr, GetSuccessCallback());

  // Explicitly respond to the first TX Power Level read command.
  test_device()->OnLEReadAdvertisingChannelTxPower();

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->le_advertising_state().enabled);
  EXPECT_EQ(new_interval.max(), test_device()->le_advertising_state().interval_max);
}

// Tests that advertising does not get enabled if the TX Power read fails.
TEST_F(HCI_LegacyLowEnergyAdvertiserTest, StartAdvertisingReadTxPowerFails) {
  AdvertisingData ad = GetExampleData();
  AdvertisingData scan_data;
  AdvertisingOptions options(kTestInterval, false, kDefaultNoAdvFlags, true);

  // Simulate failure for Read TX Power operation.
  test_device()->SetDefaultResponseStatus(kLEReadAdvertisingChannelTxPower,
                                          hci::StatusCode::kHardwareFailure);

  advertiser()->StartAdvertising(kRandomAddress, ad, scan_data, options, nullptr,
                                 GetErrorCallback());
  RunLoopUntilIdle();
  auto status = MoveLastStatus();
  ASSERT_TRUE(status);
  EXPECT_EQ(HostError::kProtocolError, status->error());
}

}  // namespace
}  // namespace hci
}  // namespace bt
