// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_layer_test.h"

namespace bt::gatt::testing {

FakeLayerTest::FakeLayerTest()
    : gatt_(std::make_unique<FakeLayer>()),
      weak_gatt_(gatt_->GetWeakPtr()),
      weak_fake_layer_(gatt_->GetFakePtr()) {}

void FakeLayerTest::TearDown() { RunLoopUntilIdle(); }

}  // namespace bt::gatt::testing
