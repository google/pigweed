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
#include "pw_bluetooth_sapphire/internal/host/gatt/fake_layer.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"

namespace bt::fidl::testing {

// Provides a common GTest harness base class for clients of the GATT layer and
// emulation of ATT behavior.
class FakeGattFixture : public bt::testing::TestLoopFixture {
 public:
  FakeGattFixture();
  ~FakeGattFixture() override = default;

  void TearDown() override;

 protected:
  const bt::gatt::GATT::WeakPtr& gatt() const {
    PW_CHECK(weak_gatt_.is_alive(),
             "fake GATT layer accessed after it was destroyed!");
    return weak_gatt_;
  }

  const bt::gatt::testing::FakeLayer::WeakPtr& fake_gatt() const {
    PW_CHECK(weak_fake_layer_.is_alive(),
             "fake GATT layer accessed after it was destroyed!");
    return weak_fake_layer_;
  }

  std::unique_ptr<bt::gatt::testing::FakeLayer> TakeGatt() {
    return std::move(gatt_);
  }

 private:
  // Store both an owning and a weak pointer to allow test code to acquire
  // ownership of the layer object for dependency injection.
  std::unique_ptr<bt::gatt::testing::FakeLayer> gatt_;
  const bt::gatt::GATT::WeakPtr weak_gatt_;
  const bt::gatt::testing::FakeLayer::WeakPtr weak_fake_layer_;
  pw::async_fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(FakeGattFixture);
};

}  // namespace bt::fidl::testing
