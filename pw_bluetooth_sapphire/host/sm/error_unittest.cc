// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/sm/error.h"

#include <gtest/gtest.h>

namespace bt::sm {
namespace {

TEST(SmErrorTest, ToString) {
  EXPECT_EQ(Error(ErrorCode::kCrossTransportKeyDerivationNotAllowed).ToString(),
            "cross-transport key dist. not allowed (SMP 0x0e)");
}

}  // namespace
}  // namespace bt::sm
