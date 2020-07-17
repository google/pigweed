// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"

namespace bthost {
namespace testing {

// This test fixture provides an instance of the Bluetooth stack with mock data plane (L2CAP) and
// GATT test doubles. The fixture is backed by a FakeController and an event loop which can be used
// to test interactions with the Bluetooth controller.
class AdapterTestFixture : public bt::testing::FakeControllerTest<bt::testing::FakeController> {
 public:
  AdapterTestFixture() = default;
  ~AdapterTestFixture() override = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  fxl::WeakPtr<bt::gap::Adapter> adapter() const { return adapter_->AsWeakPtr(); }
  bt::gatt::testing::FakeLayer* gatt() const { return gatt_.get(); }
  std::unique_ptr<bt::gatt::testing::FakeLayer> take_gatt() { return std::move(gatt_); }
  fbl::RefPtr<bt::data::testing::FakeDomain> data_domain() const { return data_plane_; }

 private:
  std::unique_ptr<bt::gap::Adapter> adapter_;
  fbl::RefPtr<bt::data::testing::FakeDomain> data_plane_;
  std::unique_ptr<bt::gatt::testing::FakeLayer> gatt_;
  inspect::Inspector inspector_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(AdapterTestFixture);
};

}  // namespace testing
}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_ADAPTER_TEST_FIXTURE_H_
