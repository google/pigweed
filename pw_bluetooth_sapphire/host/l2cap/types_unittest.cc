// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bt::l2cap {
namespace {

TEST(AnyChannelMode, ToString) {
  AnyChannelMode mode = RetransmissionAndFlowControlMode::kRetransmission;
  EXPECT_EQ(AnyChannelModeToString(mode), "(RetransmissionAndFlowControlMode) 0x01");
  mode = CreditBasedFlowControlMode::kLeCreditBasedFlowControl;
  EXPECT_EQ(AnyChannelModeToString(mode), "(CreditBasedFlowControlMode) 0x14");
}

}  // namespace
}  // namespace bt::l2cap
