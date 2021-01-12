// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_ADAPTER_TEST_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_ADAPTER_TEST_FIXTURE_H_

#include <fbl/macros.h>
#include <gtest/gtest.h>

#include "fake_adapter.h"

namespace bt::gap::testing {

class FakeAdapterTestFixture : public ::gtest::TestLoopFixture {
 public:
  FakeAdapterTestFixture() = default;
  ~FakeAdapterTestFixture() override = default;

  void SetUp() override { adapter_ = std::make_unique<bt::gap::testing::FakeAdapter>(); }

  void TearDown() override { adapter_ = nullptr; }

 protected:
  bt::gap::testing::FakeAdapter* adapter() const { return adapter_.get(); }

 private:
  std::unique_ptr<bt::gap::testing::FakeAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeAdapterTestFixture);
};

}  // namespace bt::gap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_FAKE_ADAPTER_TEST_FIXTURE_H_
