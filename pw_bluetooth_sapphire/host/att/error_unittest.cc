// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/att/error.h"

#include <gtest/gtest.h>

namespace bt::att {
namespace {

TEST(AttErrorTest, ToString) {
  EXPECT_EQ(Error(att::ErrorCode::kInvalidAttributeValueLength).ToString(),
            "invalid attribute value length (ATT 0x0d)");
}

}  // namespace
}  // namespace bt::att
