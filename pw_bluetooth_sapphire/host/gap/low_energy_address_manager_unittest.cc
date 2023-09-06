// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_address_manager.h"

#include <lib/fit/function.h>

#include "gap.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::gap {
namespace {

using testing::CommandTransaction;
using testing::MockController;

using TestingBase = testing::ControllerTest<MockController>;

const DeviceAddress kPublic(DeviceAddress::Type::kLEPublic, {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA});

class LowEnergyAddressManagerTest : public TestingBase {
 public:
  LowEnergyAddressManagerTest() = default;
  ~LowEnergyAddressManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    addr_mgr_ = std::make_unique<LowEnergyAddressManager>(
        kPublic, [this] { return IsRandomAddressChangeAllowed(); }, cmd_channel()->AsWeakPtr(),
        pw_dispatcher_);
    ASSERT_EQ(kPublic, addr_mgr()->identity_address());
    ASSERT_FALSE(addr_mgr()->irk());
    addr_mgr_->register_address_changed_callback([&](auto) { address_changed_cb_count_++; });
  }

  void TearDown() override {
    addr_mgr_ = nullptr;
    TestingBase::TearDown();
  }

  DeviceAddress EnsureLocalAddress() {
    bool called = false;
    DeviceAddress result;
    addr_mgr()->EnsureLocalAddress([&](const auto& addr) {
      result = addr;
      called = true;
    });
    RunLoopUntilIdle();
    EXPECT_TRUE(called);
    return result;
  }

  // Called by |addr_mgr_|.
  bool IsRandomAddressChangeAllowed() const { return random_address_change_allowed_; }

  LowEnergyAddressManager* addr_mgr() const { return addr_mgr_.get(); }

  void set_random_address_change_allowed(bool value) { random_address_change_allowed_ = value; }

  size_t address_changed_cb_count() const { return address_changed_cb_count_; }

 private:
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};
  std::unique_ptr<LowEnergyAddressManager> addr_mgr_;
  bool random_address_change_allowed_ = true;
  size_t address_changed_cb_count_ = 0;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LowEnergyAddressManagerTest);
};

TEST_F(LowEnergyAddressManagerTest, DefaultState) { EXPECT_EQ(kPublic, EnsureLocalAddress()); }

TEST_F(LowEnergyAddressManagerTest, EnablePrivacy) {
  // Respond with success.
  const StaticByteBuffer kResponse(0x0E, 4,     // Command Complete, 4 bytes,
                                   1,           // 1 allowed packet
                                   0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                   0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kResponse);

  const UInt128 kIrk{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  int hci_cmd_count = 0;
  DeviceAddress addr;
  test_device()->SetTransactionCallback([&](const auto& rx) {
    hci_cmd_count++;

    const auto addr_bytes = rx.view(sizeof(hci_spec::CommandHeader));
    ASSERT_EQ(6u, addr_bytes.size());
    addr = DeviceAddress(DeviceAddress::Type::kLERandom, DeviceAddressBytes(addr_bytes));
  });

  addr_mgr()->set_irk(kIrk);
  ASSERT_TRUE(addr_mgr()->irk());
  EXPECT_EQ(kIrk, *addr_mgr()->irk());
  EXPECT_FALSE(addr_mgr()->PrivacyEnabled());

  addr_mgr()->EnablePrivacy(true);
  // Privacy is now considered enabled.
  // Even though Privacy is enabled, the LE address has not been changed so no notifications.
  EXPECT_EQ(address_changed_cb_count(), 0u);
  // Further requests to enable should not trigger additional HCI commands.
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  EXPECT_TRUE(addr_mgr()->PrivacyEnabled());
  // We should have received a HCI command with a RPA resolvable using |kIrk|.
  EXPECT_EQ(1, hci_cmd_count);
  EXPECT_TRUE(addr.IsResolvablePrivate());
  EXPECT_TRUE(sm::util::IrkCanResolveRpa(kIrk, addr));
  // The new random address should be returned.
  EXPECT_EQ(addr, EnsureLocalAddress());
  // The address is updated so the listener should be notified.
  EXPECT_EQ(address_changed_cb_count(), 1u);

  // Assign a new IRK. The new address should be used when it gets refreshed.
  // Re-enable privacy with a new IRK. The latest IRK should be used.
  const UInt128 kIrk2{{15, 14, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1}};
  addr_mgr()->set_irk(kIrk2);
  ASSERT_TRUE(addr_mgr()->irk());
  EXPECT_EQ(kIrk2, *addr_mgr()->irk());

  // Returns the same address.
  EXPECT_EQ(addr, EnsureLocalAddress());
  EXPECT_FALSE(sm::util::IrkCanResolveRpa(kIrk2, addr));

  // Re-enable privacy to trigger a refresh.
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kResponse);
  addr_mgr()->EnablePrivacy(false);
  // Disabling Privacy should result in the Public address being used so we expect a notification.
  EXPECT_EQ(address_changed_cb_count(), 2u);
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  EXPECT_EQ(addr, EnsureLocalAddress());
  EXPECT_TRUE(addr.IsResolvablePrivate());
  EXPECT_TRUE(sm::util::IrkCanResolveRpa(kIrk2, addr));
  EXPECT_EQ(address_changed_cb_count(), 3u);
}

TEST_F(LowEnergyAddressManagerTest, EnablePrivacyNoIrk) {
  // Respond with success.
  const StaticByteBuffer kResponse(0x0E, 4,     // Command Complete, 4 bytes,
                                   1,           // 1 allowed packet
                                   0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                   0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kResponse);

  int hci_cmd_count = 0;
  DeviceAddress addr;
  test_device()->SetTransactionCallback([&](const auto& rx) {
    hci_cmd_count++;

    const auto addr_bytes = rx.view(sizeof(hci_spec::CommandHeader));
    ASSERT_EQ(6u, addr_bytes.size());
    addr = DeviceAddress(DeviceAddress::Type::kLERandom, DeviceAddressBytes(addr_bytes));
  });

  addr_mgr()->EnablePrivacy(true);

  // Privacy is now considered enabled. Further requests to enable should not
  // trigger additional HCI commands.
  EXPECT_EQ(address_changed_cb_count(), 0u);
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  // We should have received a HCI command with a NRPA.
  EXPECT_EQ(1, hci_cmd_count);
  EXPECT_TRUE(addr.IsNonResolvablePrivate());

  // The new random address should be returned.
  EXPECT_EQ(addr, EnsureLocalAddress());
  EXPECT_EQ(address_changed_cb_count(), 1u);
}

TEST_F(LowEnergyAddressManagerTest, EnablePrivacyHciError) {
  // Respond with error.
  const StaticByteBuffer kErrorResponse(0x0E, 4,     // Command Complete, 4 bytes,
                                        1,           // 1 allowed packet
                                        0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                        0x0C         // status: Command Disallowed
  );
  // The second time respond with success.
  const StaticByteBuffer kSuccessResponse(0x0E, 4,     // Command Complete, 4 bytes,
                                          1,           // 1 allowed packet
                                          0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                          0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kErrorResponse);
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  addr_mgr()->EnablePrivacy(true);

  // Request the new address and run the event loop. The old address should be
  // returned due to the failure.
  EXPECT_EQ(kPublic, EnsureLocalAddress());
  // Address hasn't changed so no notifications.
  EXPECT_EQ(address_changed_cb_count(), 0u);

  // Requesting the address a second time while address update is disallowed
  // should return the old address without sending HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });
  set_random_address_change_allowed(false);
  EXPECT_EQ(kPublic, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);
  // Address hasn't changed so no notifications.
  EXPECT_EQ(address_changed_cb_count(), 0u);

  // Requesting the address a third time while address update is allowed should
  // configure and return the new address.
  set_random_address_change_allowed(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  EXPECT_EQ(1, hci_count);
  EXPECT_EQ(address_changed_cb_count(), 1u);
}

TEST_F(LowEnergyAddressManagerTest, EnablePrivacyWhileAddressChangeIsDisallowed) {
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });
  set_random_address_change_allowed(false);

  // No HCI commands should be sent while disallowed.
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_EQ(0, hci_count);

  EXPECT_TRUE(addr_mgr()->PrivacyEnabled());
  EXPECT_EQ(kPublic, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);
  // Address hasn't changed so no notifications.
  EXPECT_EQ(address_changed_cb_count(), 0u);

  // Requesting the address while address change is allowed should configure and
  // return the new address.
  set_random_address_change_allowed(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  EXPECT_EQ(1, hci_count);
  // Address has changed.
  EXPECT_EQ(address_changed_cb_count(), 1u);
}

TEST_F(LowEnergyAddressManagerTest, AddressExpiration) {
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  addr_mgr()->EnablePrivacy(true);
  auto addr1 = EnsureLocalAddress();
  EXPECT_TRUE(addr1.IsNonResolvablePrivate());
  // Address has changed.
  EXPECT_EQ(address_changed_cb_count(), 1u);

  // Requesting the address again should keep returning the same address without
  // sending any HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);
  // Address hasn't changed so no notifications.
  EXPECT_EQ(address_changed_cb_count(), 1u);

  // A new address should be generated and configured after the random address
  // interval.
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(1, hci_count);
  // Address has changed due to timeout.
  EXPECT_EQ(address_changed_cb_count(), 2u);

  // Requesting the address again should return the new address.
  auto addr2 = EnsureLocalAddress();
  EXPECT_TRUE(addr2.IsNonResolvablePrivate());
  EXPECT_NE(addr1, addr2);
  EXPECT_EQ(1, hci_count);
  // The new address was already reported after timeout so no other notification.
  EXPECT_EQ(address_changed_cb_count(), 2u);
}

TEST_F(LowEnergyAddressManagerTest, AddressExpirationWhileAddressChangeIsDisallowed) {
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  addr_mgr()->EnablePrivacy(true);
  auto addr1 = EnsureLocalAddress();
  EXPECT_TRUE(addr1.IsNonResolvablePrivate());

  // Requesting the address again should keep returning the same address without
  // sending any HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);

  // After the interval ends, the address should be marked as expired but should
  // not send an HCI command while the command is disallowed.
  set_random_address_change_allowed(false);
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);

  // Requesting the address again while the command is allowed should configure
  // and return the new address.
  set_random_address_change_allowed(true);
  auto addr2 = EnsureLocalAddress();
  EXPECT_TRUE(addr2.IsNonResolvablePrivate());
  EXPECT_NE(addr1, addr2);
  EXPECT_EQ(1, hci_count);
}

TEST_F(LowEnergyAddressManagerTest, DisablePrivacy) {
  // Enable privacy.
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  addr_mgr()->EnablePrivacy(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  // Address has changed to an NRPA.
  EXPECT_EQ(address_changed_cb_count(), 1u);

  // Disable privacy.
  addr_mgr()->EnablePrivacy(false);

  // The public address should be returned for the local address.
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
  // Address has change to public since Privacy is disabled.
  EXPECT_EQ(address_changed_cb_count(), 2u);

  // No HCI commands should get sent after private address interval expires.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(0, hci_count);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
}

TEST_F(LowEnergyAddressManagerTest, DisablePrivacyDuringAddressChange) {
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; });

  // Enable and disable in quick succession. HCI command should be sent but the
  // local address shouldn't take effect.
  addr_mgr()->EnablePrivacy(true);
  addr_mgr()->EnablePrivacy(false);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
  EXPECT_EQ(1, hci_count);

  // No HCI commands should get sent after private address interval expires.
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(1, hci_count);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
}

TEST_F(LowEnergyAddressManagerTest, MultipleAddressChangedCallbacksAreNotified) {
  // The |LowEnergyAddressManagerTest| registers an address changed callback on construction.
  // Register another.
  size_t cb_count2 = 0;
  addr_mgr()->register_address_changed_callback([&](auto) { cb_count2++; });

  // Enable privacy.
  const auto kSuccessResponse = StaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                                                 1,           // 1 allowed packet
                                                 0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                                                 0x00         // status: success
  );
  EXPECT_CMD_PACKET_OUT(test_device(), hci_spec::kLESetRandomAddress, &kSuccessResponse);

  addr_mgr()->EnablePrivacy(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  // Address has changed to an NRPA. Both callbacks should be notified.
  EXPECT_EQ(address_changed_cb_count(), 1u);
  EXPECT_EQ(cb_count2, 1u);

  // Before privacy is disabled, another callback is registered.
  size_t cb_count3 = 0;
  addr_mgr()->register_address_changed_callback([&](auto) { cb_count3++; });
  EXPECT_EQ(cb_count3, 0u);

  // Disable privacy.
  addr_mgr()->EnablePrivacy(false);

  // The public address should be returned for the local address.
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
  // Address has changed - all callbacks should be notified.
  EXPECT_EQ(address_changed_cb_count(), 2u);
  EXPECT_EQ(cb_count2, 2u);
  EXPECT_EQ(cb_count3, 1u);
}

}  // namespace
}  // namespace bt::gap
