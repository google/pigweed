// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::gatt::testing {

// Provides a common GTest harness base class for clients of the GATT layer and emulation of
// ATT behavior.
class FakeLayerTest : public ::gtest::TestLoopFixture {
 public:
  FakeLayerTest();
  ~FakeLayerTest() override = default;

  void TearDown() override;

 protected:
  const GATT::WeakPtr& gatt() const {
    BT_ASSERT_MSG(weak_gatt_.is_alive(), "fake GATT layer accessed after it was destroyed!");
    return weak_gatt_;
  }

  const FakeLayer::WeakPtr& fake_gatt() const {
    BT_ASSERT_MSG(weak_fake_layer_.is_alive(), "fake GATT layer accessed after it was destroyed!");
    return weak_fake_layer_;
  }

  std::unique_ptr<FakeLayer> TakeGatt() { return std::move(gatt_); }

 private:
  // Store both an owning and a weak pointer to allow test code to acquire ownership of the layer
  // object for dependency injection.
  std::unique_ptr<FakeLayer> gatt_;
  const GATT::WeakPtr weak_gatt_;
  const FakeLayer::WeakPtr weak_fake_layer_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(FakeLayerTest);
};

}  // namespace bt::gatt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_
