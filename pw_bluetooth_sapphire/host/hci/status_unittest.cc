// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

#include <gtest/gtest.h>

namespace bt {
namespace hci {
namespace {

TEST(HCI_StatusTest, ProtocolSuccess) {
  Status status(StatusCode::kSuccess);
  EXPECT_TRUE(status);
  EXPECT_FALSE(status.is_protocol_error());
}

TEST(HCI_StatusTest, ProtocolError) {
  Status status(StatusCode::kHardwareFailure);
  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(StatusCode::kHardwareFailure, status.protocol_error());
}

}  // namespace
}  // namespace hci
}  // namespace bt
