// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "adapter_test_fixture.h"

namespace bthost {
namespace testing {

using bt::testing::FakeController;
using TestingBase = bt::testing::FakeControllerTest<FakeController>;

void AdapterTestFixture::SetUp() {
  TestingBase::SetUp();

  data_plane_ = bt::data::testing::FakeDomain::Create();
  data_plane_->Initialize();
  gatt_ = bt::gatt::testing::FakeLayer::Create();
  adapter_ = std::make_unique<bt::gap::Adapter>(transport(), gatt_, data_plane_);

  FakeController::Settings settings;
  settings.ApplyDualModeDefaults();
  test_device()->set_settings(settings);
  StartTestDevice();

  bool success = false;
  adapter_->Initialize([&](bool result) { success = result; }, [] {});
  RunLoopUntilIdle();
  ASSERT_TRUE(success);
}

void AdapterTestFixture::TearDown() {
  // Drain all scheduled tasks.
  RunLoopUntilIdle();

  // Cleanly shut down the stack.
  adapter_ = nullptr;
  RunLoopUntilIdle();

  gatt_ = nullptr;
  data_plane_ = nullptr;
  TestingBase::TearDown();
}

}  // namespace testing
}  // namespace bthost
