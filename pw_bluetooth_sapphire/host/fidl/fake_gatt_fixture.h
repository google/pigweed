// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_GATT_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_GATT_FIXTURE_H_

#include <pw_async_fuchsia/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::fidl::testing {

// Provides a common GTest harness base class for clients of the GATT layer and emulation of
// ATT behavior.
class FakeGattFixture : public ::gtest::TestLoopFixture {
 public:
  FakeGattFixture();
  ~FakeGattFixture() override = default;

  void TearDown() override;

 protected:
  const bt::gatt::GATT::WeakPtr& gatt() const {
    BT_ASSERT_MSG(weak_gatt_.is_alive(), "fake GATT layer accessed after it was destroyed!");
    return weak_gatt_;
  }

  const bt::gatt::testing::FakeLayer::WeakPtr& fake_gatt() const {
    BT_ASSERT_MSG(weak_fake_layer_.is_alive(), "fake GATT layer accessed after it was destroyed!");
    return weak_fake_layer_;
  }

  std::unique_ptr<bt::gatt::testing::FakeLayer> TakeGatt() { return std::move(gatt_); }

 private:
  // Store both an owning and a weak pointer to allow test code to acquire ownership of the layer
  // object for dependency injection.
  std::unique_ptr<bt::gatt::testing::FakeLayer> gatt_;
  const bt::gatt::GATT::WeakPtr weak_gatt_;
  const bt::gatt::testing::FakeLayer::WeakPtr weak_fake_layer_;
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(FakeGattFixture);
};

}  // namespace bt::fidl::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_GATT_FIXTURE_H_
