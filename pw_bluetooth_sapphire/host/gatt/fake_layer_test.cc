// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_layer_test.h"

namespace bt {
namespace gatt {
namespace testing {

FakeLayerTest::FakeLayerTest()
    : gatt_(std::make_unique<FakeLayer>()), weak_gatt_(gatt_->AsWeakPtr()) {}

void FakeLayerTest::TearDown() { RunLoopUntilIdle(); }

}  // namespace testing
}  // namespace gatt
}  // namespace bt
