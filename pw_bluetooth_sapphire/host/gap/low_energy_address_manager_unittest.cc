// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_address_manager.h"

#include <fbl/function.h>

#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

#include "gap.h"

namespace bt {
namespace gap {
namespace {

using common::CreateStaticByteBuffer;
using common::DeviceAddress;
using common::DeviceAddressBytes;
using common::UInt128;
using testing::CommandTransaction;
using testing::TestController;

using TestingBase = testing::FakeControllerTest<TestController>;

const DeviceAddress kPublic(DeviceAddress::Type::kLEPublic,
                            "AA:BB:CC:DD:EE:FF");

class GAP_LowEnergyAddressManagerTest : public TestingBase {
 public:
  GAP_LowEnergyAddressManagerTest() = default;
  ~GAP_LowEnergyAddressManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    addr_mgr_ = std::make_unique<LowEnergyAddressManager>(
        kPublic, [this] { return IsRandomAddressChangeAllowed(); },
        transport());
    ASSERT_EQ(kPublic, addr_mgr()->identity_address());
    ASSERT_FALSE(addr_mgr()->irk());
    StartTestDevice();
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
  bool IsRandomAddressChangeAllowed() const {
    return random_address_change_allowed_;
  }

  LowEnergyAddressManager* addr_mgr() const { return addr_mgr_.get(); };

  void set_random_address_change_allowed(bool value) {
    random_address_change_allowed_ = value;
  }

 private:
  std::unique_ptr<LowEnergyAddressManager> addr_mgr_;
  bool random_address_change_allowed_ = true;

  DISALLOW_COPY_ASSIGN_AND_MOVE(GAP_LowEnergyAddressManagerTest);
};

TEST_F(GAP_LowEnergyAddressManagerTest, DefaultState) {
  EXPECT_EQ(kPublic, EnsureLocalAddress());
}

TEST_F(GAP_LowEnergyAddressManagerTest, EnablePrivacy) {
  // Respond with success.
  const auto kResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kResponse}));

  const UInt128 kIrk{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
  int hci_cmd_count = 0;
  DeviceAddress addr;
  test_device()->SetTransactionCallback(
      [&](const auto& rx) {
        hci_cmd_count++;

        const auto addr_bytes = rx.view(sizeof(hci::CommandHeader));
        ASSERT_EQ(6u, addr_bytes.size());
        addr = DeviceAddress(DeviceAddress::Type::kLERandom,
                             DeviceAddressBytes(addr_bytes));
      },
      dispatcher());

  addr_mgr()->set_irk(kIrk);
  ASSERT_TRUE(addr_mgr()->irk());
  EXPECT_EQ(kIrk, *addr_mgr()->irk());
  addr_mgr()->EnablePrivacy(true);

  // Privacy is now considered enabled. Further requests to enable should not
  // trigger additional HCI commands.
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  // We should have received a HCI command with a RPA resolvable using |kIrk|.
  EXPECT_EQ(1, hci_cmd_count);
  EXPECT_TRUE(addr.IsResolvablePrivate());
  EXPECT_TRUE(sm::util::IrkCanResolveRpa(kIrk, addr));

  // The new random address should be returned.
  EXPECT_EQ(addr, EnsureLocalAddress());

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
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kResponse}));
  addr_mgr()->EnablePrivacy(false);
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  EXPECT_EQ(addr, EnsureLocalAddress());
  EXPECT_TRUE(addr.IsResolvablePrivate());
  EXPECT_TRUE(sm::util::IrkCanResolveRpa(kIrk2, addr));
}

TEST_F(GAP_LowEnergyAddressManagerTest, EnablePrivacyNoIrk) {
  // Respond with success.
  const auto kResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kResponse}));

  int hci_cmd_count = 0;
  DeviceAddress addr;
  test_device()->SetTransactionCallback(
      [&](const auto& rx) {
        hci_cmd_count++;

        const auto addr_bytes = rx.view(sizeof(hci::CommandHeader));
        ASSERT_EQ(6u, addr_bytes.size());
        addr = DeviceAddress(DeviceAddress::Type::kLERandom,
                             DeviceAddressBytes(addr_bytes));
      },
      dispatcher());

  addr_mgr()->EnablePrivacy(true);

  // Privacy is now considered enabled. Further requests to enable should not
  // trigger additional HCI commands.
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();

  // We should have received a HCI command with a NRPA.
  EXPECT_EQ(1, hci_cmd_count);
  EXPECT_TRUE(addr.IsNonResolvablePrivate());

  // The new random address should be returned.
  EXPECT_EQ(addr, EnsureLocalAddress());
}

TEST_F(GAP_LowEnergyAddressManagerTest, EnablePrivacyHciError) {
  // Respond with error.
  const auto kErrorResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x0C         // status: Command Disallowed
      );
  // The second time respond with success.
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kErrorResponse}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  addr_mgr()->EnablePrivacy(true);

  // Request the new address and run the event loop. The old address should be
  // returned due to the failure.
  EXPECT_EQ(kPublic, EnsureLocalAddress());

  // Requesting the address a second time while address update is disallowed
  // should return the old address without sending HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());
  set_random_address_change_allowed(false);
  EXPECT_EQ(kPublic, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);

  // Requesting the address a third time while address update is allowed should
  // configure and return the new address.
  set_random_address_change_allowed(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  EXPECT_EQ(1, hci_count);
}

TEST_F(GAP_LowEnergyAddressManagerTest,
       EnablePrivacyWhileAddressChangeIsDisallowed) {
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());
  set_random_address_change_allowed(false);

  // No HCI commands should be sent while disallowed.
  addr_mgr()->EnablePrivacy(true);
  RunLoopUntilIdle();
  EXPECT_EQ(0, hci_count);

  EXPECT_EQ(kPublic, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);

  // Requesting the address while address change is allowed should configure and
  // return the new address.
  set_random_address_change_allowed(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());
  EXPECT_EQ(1, hci_count);
}

TEST_F(GAP_LowEnergyAddressManagerTest, AddressExpiration) {
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  addr_mgr()->EnablePrivacy(true);
  auto addr1 = EnsureLocalAddress();
  EXPECT_TRUE(addr1.IsNonResolvablePrivate());

  // Requesting the address again should keep returning the same address without
  // sending any HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(addr1, EnsureLocalAddress());
  EXPECT_EQ(0, hci_count);

  // A new address should be generated and configured after the random address
  // interval.
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(1, hci_count);

  // Requesting the address again should return the new address.
  auto addr2 = EnsureLocalAddress();
  EXPECT_TRUE(addr2.IsNonResolvablePrivate());
  EXPECT_NE(addr1, addr2);
  EXPECT_EQ(1, hci_count);
}

TEST_F(GAP_LowEnergyAddressManagerTest,
       AddressExpirationWhileAddressChangeIsDisallowed) {
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  addr_mgr()->EnablePrivacy(true);
  auto addr1 = EnsureLocalAddress();
  EXPECT_TRUE(addr1.IsNonResolvablePrivate());

  // Requesting the address again should keep returning the same address without
  // sending any HCI commands.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());
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

TEST_F(GAP_LowEnergyAddressManagerTest, DisablePrivacy) {
  // Enable privacy.
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  addr_mgr()->EnablePrivacy(true);
  EXPECT_TRUE(EnsureLocalAddress().IsNonResolvablePrivate());

  // Disable privacy.
  addr_mgr()->EnablePrivacy(false);

  // The public address should be returned for the local address.
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());

  // No HCI commands should get sent after private address interval expires.
  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());
  RunLoopFor(kPrivateAddressTimeout);
  EXPECT_EQ(0, hci_count);
  EXPECT_EQ(DeviceAddress::Type::kLEPublic, EnsureLocalAddress().type());
}

TEST_F(GAP_LowEnergyAddressManagerTest, DisablePrivacyDuringAddressChange) {
  const auto kSuccessResponse =
      CreateStaticByteBuffer(0x0E, 4,     // Command Complete, 4 bytes,
                             1,           // 1 allowed packet
                             0x05, 0x20,  // opcode: HCI_LE_Set_Random_Address
                             0x00         // status: success
      );
  test_device()->QueueCommandTransaction(
      CommandTransaction(hci::kLESetRandomAddress, {&kSuccessResponse}));

  int hci_count = 0;
  test_device()->SetTransactionCallback([&] { hci_count++; }, dispatcher());

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

}  // namespace
}  // namespace gap
}  // namespace bt
