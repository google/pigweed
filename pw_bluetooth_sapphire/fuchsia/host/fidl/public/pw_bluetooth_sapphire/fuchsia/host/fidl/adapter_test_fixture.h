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

#include <pw_async_fuchsia/dispatcher.h>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/adapter.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/fake_layer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_l2cap.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"

namespace bthost::testing {

// This test fixture provides an instance of the Bluetooth stack with mock data
// plane (L2CAP) and GATT test doubles. The fixture is backed by a
// FakeController and an event loop which can be used to test interactions with
// the Bluetooth controller.
class AdapterTestFixture
    : public bt::testing::TestLoopFixture,
      public bt::testing::ControllerTest<bt::testing::FakeController> {
 public:
  AdapterTestFixture()
      : bt::testing::ControllerTest<bt::testing::FakeController>(
            pw_dispatcher_),
        pw_dispatcher_(dispatcher()) {}
  ~AdapterTestFixture() override = default;

  pw::async::Dispatcher& pw_dispatcher() { return pw_dispatcher_; }

 protected:
  void SetUp() override;
  void SetUp(bt::testing::FakeController::Settings settings,
             pw::bluetooth::Controller::FeaturesBits features =
                 pw::bluetooth::Controller::FeaturesBits{0});
  void TearDown() override;

  bt::gap::Adapter::WeakPtr adapter() const { return adapter_->AsWeakPtr(); }
  bt::gatt::testing::FakeLayer* gatt() const { return gatt_.get(); }
  std::unique_ptr<bt::gatt::testing::FakeLayer> take_gatt() {
    return std::move(gatt_);
  }
  bt::l2cap::testing::FakeL2cap* l2cap() const { return l2cap_; }

 private:
  pw::async_fuchsia::FuchsiaDispatcher pw_dispatcher_;
  std::unique_ptr<bt::gap::Adapter> adapter_;
  bt::l2cap::testing::FakeL2cap* l2cap_;
  std::unique_ptr<bt::gatt::testing::FakeLayer> gatt_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(AdapterTestFixture);
};

}  // namespace bthost::testing
