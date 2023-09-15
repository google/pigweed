// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_ADAPTER_TEST_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_ADAPTER_TEST_FIXTURE_H_

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_adapter.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::fidl::testing {

class FakeAdapterTestFixture : public ::gtest::TestLoopFixture {
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
  pw::async::fuchsia::FuchsiaDispatcher dispatcher_{dispatcher()};

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeAdapterTestFixture);
};

}  // namespace bt::fidl::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_FAKE_ADAPTER_TEST_FIXTURE_H_
