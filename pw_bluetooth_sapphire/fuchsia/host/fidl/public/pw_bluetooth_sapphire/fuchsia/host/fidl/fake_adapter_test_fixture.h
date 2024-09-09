// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include "pw_async_fuchsia/dispatcher.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/fake_adapter.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"
#include "pw_unit_test/framework.h"

namespace bt::fidl::testing {

class FakeAdapterTestFixture : public bt::testing::TestLoopFixture {
 public:
  FakeAdapterTestFixture() = default;
  ~FakeAdapterTestFixture() override = default;

  void SetUp() override {
    adapter_ = std::make_unique<bt::gap::testing::FakeAdapter>(pw_dispatcher());
  }

  void TearDown() override { adapter_ = nullptr; }

  pw::async::Dispatcher& pw_dispatcher() { return dispatcher_; }

 protected:
  bt::gap::testing::FakeAdapter* adapter() const { return adapter_.get(); }

 private:
  std::unique_ptr<bt::gap::testing::FakeAdapter> adapter_;
  pw::async_fuchsia::FuchsiaDispatcher dispatcher_{dispatcher()};

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeAdapterTestFixture);
};

}  // namespace bt::fidl::testing
