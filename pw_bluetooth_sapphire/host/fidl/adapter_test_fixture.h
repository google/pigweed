// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_

#include <pw_async_fuchsia/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bthost::testing {

// This test fixture provides an instance of the Bluetooth stack with mock data plane (L2CAP) and
// GATT test doubles. The fixture is backed by a FakeController and an event loop which can be used
// to test interactions with the Bluetooth controller.
class AdapterTestFixture : public ::gtest::TestLoopFixture,
                           public bt::testing::ControllerTest<bt::testing::FakeController> {
 public:
  AdapterTestFixture()
      : bt::testing::ControllerTest<bt::testing::FakeController>(pw_dispatcher_),
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
  std::unique_ptr<bt::gatt::testing::FakeLayer> take_gatt() { return std::move(gatt_); }
  bt::l2cap::testing::FakeL2cap* l2cap() const { return l2cap_; }

 private:
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_;
  std::unique_ptr<bt::gap::Adapter> adapter_;
  bt::l2cap::testing::FakeL2cap* l2cap_;
  std::unique_ptr<bt::gatt::testing::FakeLayer> gatt_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(AdapterTestFixture);
};

}  // namespace bthost::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_
