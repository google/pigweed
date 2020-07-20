// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_

#include <lib/gtest/test_loop_fixture.h>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace gatt {
namespace testing {

// Provides a common GTest harness base class for clients of the GATT layer and emulation of
// ATT behavior.
class FakeLayerTest : public ::gtest::TestLoopFixture {
 public:
  FakeLayerTest();
  ~FakeLayerTest() override = default;

  void TearDown() override;

 protected:
  FakeLayer* gatt() const {
    auto* ptr = static_cast<FakeLayer*>(weak_gatt_.get());
    ZX_ASSERT_MSG(ptr, "fake GATT layer accessed after it was destroyed!");
    return ptr;
  }

  std::unique_ptr<FakeLayer> TakeGatt() { return std::move(gatt_); }

 private:
  // Store both an owning and a weak pointer to allow test code to acquire ownership of the layer
  // object for dependency injection.
  std::unique_ptr<FakeLayer> gatt_;
  const fxl::WeakPtr<GATT> weak_gatt_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(FakeLayerTest);
};

}  // namespace testing
}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_TEST_H_
