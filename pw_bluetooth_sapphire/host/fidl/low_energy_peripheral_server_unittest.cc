// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/fidl/low_energy_peripheral_server.h"

#include "adapter_test_fixture.h"

namespace bthost {
namespace {

namespace fble = fuchsia::bluetooth::le;

class FIDL_LowEnergyPeripheralServerTest : public bthost::testing::AdapterTestFixture {
 public:
  FIDL_LowEnergyPeripheralServerTest() = default;
  ~FIDL_LowEnergyPeripheralServerTest() override = default;

  void SetUp() override {
    AdapterTestFixture::SetUp();

    // Create a LowEnergyPeripheralServer and bind it to a local client.
    fidl::InterfaceHandle<fble::Peripheral> handle;
    server_ = std::make_unique<LowEnergyPeripheralServer>(adapter(), handle.NewRequest());
    proxy_.Bind(std::move(handle));
  }

  void TearDown() override {
    RunLoopUntilIdle();

    proxy_ = nullptr;
    server_ = nullptr;
    AdapterTestFixture::TearDown();
  }

  LowEnergyPeripheralServer* server() const { return server_.get(); }

 private:
  std::unique_ptr<LowEnergyPeripheralServer> server_;
  fble::PeripheralPtr proxy_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_LowEnergyPeripheralServerTest);
};

// Tests that aborting a StartAdvertising command sequence does not cause a crash in successive
// requests.
TEST_F(FIDL_LowEnergyPeripheralServerTest, StartAdvertisingWhilePendingDoesNotCrash) {
  fble::AdvertisingParameters params1, params2, params3;
  fidl::InterfaceHandle<fble::AdvertisingHandle> token1, token2, token3;

  std::optional<fit::result<void, fble::PeripheralError>> result1, result2, result3;
  server()->StartAdvertising(std::move(params1), token1.NewRequest(),
                             [&](auto result) { result1 = std::move(result); });
  server()->StartAdvertising(std::move(params2), token2.NewRequest(),
                             [&](auto result) { result2 = std::move(result); });
  server()->StartAdvertising(std::move(params3), token3.NewRequest(),
                             [&](auto result) { result3 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result3);
  EXPECT_TRUE(result1->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result1->error());
  EXPECT_TRUE(result2->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result2->error());
  EXPECT_TRUE(result3->is_ok());
}

// Same as the test above but tests that an error status leaves the server in the expected state.
TEST_F(FIDL_LowEnergyPeripheralServerTest,
       StartAdvertisingWhilePendingDoesNotCrashWithControllerError) {
  test_device()->SetDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable,
                                          bt::hci::StatusCode::kCommandDisallowed);
  fble::AdvertisingParameters params1, params2, params3, params4;
  fidl::InterfaceHandle<fble::AdvertisingHandle> token1, token2, token3, token4;

  std::optional<fit::result<void, fble::PeripheralError>> result1, result2, result3, result4;
  server()->StartAdvertising(std::move(params1), token1.NewRequest(),
                             [&](auto result) { result1 = std::move(result); });
  server()->StartAdvertising(std::move(params2), token2.NewRequest(),
                             [&](auto result) { result2 = std::move(result); });
  server()->StartAdvertising(std::move(params3), token3.NewRequest(),
                             [&](auto result) { result3 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result3);
  EXPECT_TRUE(result1->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result1->error());
  EXPECT_TRUE(result2->is_error());
  EXPECT_EQ(fble::PeripheralError::ABORTED, result2->error());
  EXPECT_TRUE(result3->is_error());
  EXPECT_EQ(fble::PeripheralError::FAILED, result3->error());

  // The next request should succeed as normal.
  test_device()->ClearDefaultResponseStatus(bt::hci::kLESetAdvertisingEnable);
  server()->StartAdvertising(std::move(params4), token4.NewRequest(),
                             [&](auto result) { result4 = std::move(result); });
  RunLoopUntilIdle();

  ASSERT_TRUE(result4);
  EXPECT_TRUE(result4->is_ok());
}

}  // namespace
}  // namespace bthost
