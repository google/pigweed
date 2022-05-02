// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter_test_fixture.h"

namespace bthost::testing {

using bt::testing::FakeController;
using TestingBase = bt::testing::ControllerTest<FakeController>;

void AdapterTestFixture::SetUp() {
  TestingBase::SetUp();

  auto l2cap = std::make_unique<bt::l2cap::testing::FakeL2cap>();
  l2cap_ = l2cap.get();
  gatt_ = std::make_unique<bt::gatt::testing::FakeLayer>();
  adapter_ = bt::gap::Adapter::Create(transport()->WeakPtr(), gatt_->AsWeakPtr(), std::move(l2cap));

  FakeController::Settings settings;
  settings.ApplyDualModeDefaults();
  test_device()->set_settings(settings);
  StartTestDevice();

  bool success = false;
  adapter_->Initialize([&](bool result) { success = result; }, [] {});
  RunLoopUntilIdle();
  ASSERT_TRUE(success);
  ASSERT_TRUE(adapter_->le());
  ASSERT_TRUE(adapter_->bredr());
}

void AdapterTestFixture::TearDown() {
  // Drain all scheduled tasks.
  RunLoopUntilIdle();

  // Cleanly shut down the stack.
  l2cap_ = nullptr;
  adapter_ = nullptr;
  RunLoopUntilIdle();

  gatt_ = nullptr;
  TestingBase::TearDown();
}

}  // namespace bthost::testing
