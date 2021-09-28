// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

#include <gtest/gtest.h>

namespace bt::hci {
namespace {

TEST(StatusTest, ProtocolSuccess) {
  Status status(hci_spec::StatusCode::kSuccess);
  EXPECT_TRUE(status);
  EXPECT_FALSE(status.is_protocol_error());
}

TEST(StatusTest, ProtocolError) {
  Status status(hci_spec::StatusCode::kHardwareFailure);
  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci_spec::StatusCode::kHardwareFailure, status.protocol_error());
}

}  // namespace
}  // namespace bt::hci
