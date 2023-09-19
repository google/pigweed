// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_gatt_fixture.h"

namespace bt::fidl::testing {

FakeGattFixture::FakeGattFixture()
    : gatt_(std::make_unique<bt::gatt::testing::FakeLayer>(pw_dispatcher_)),
      weak_gatt_(gatt_->GetWeakPtr()),
      weak_fake_layer_(gatt_->GetFakePtr()) {}

void FakeGattFixture::TearDown() { RunLoopUntilIdle(); }

}  // namespace bt::fidl::testing
