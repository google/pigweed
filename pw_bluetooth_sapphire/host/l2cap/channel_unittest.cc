// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel.h"

#include <gtest/gtest.h>

#include "fake_channel.h"

namespace bt::l2cap::testing {

TEST(ChannelTest, UniqueId) {
  // Same handle + Same local id = Same unique id
  auto channel =
      std::make_unique<FakeChannel>(/*id=*/1, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  auto chan_diff_remote =
      std::make_unique<FakeChannel>(/*id=*/1, /*remote_id=*/2, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_EQ(channel->unique_id(), chan_diff_remote->unique_id());

  // Same handle + Different local id = Different unique id
  auto chan_diff_local =
      std::make_unique<FakeChannel>(/*id=*/2, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff_local->unique_id());

  // Same handle + Same local id = Same unique id
  auto chan_same =
      std::make_unique<FakeChannel>(/*id=*/1, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_EQ(channel->unique_id(), chan_same->unique_id());

  // Different handle + Same local id = Different unique id
  auto chan_diff_conn =
      std::make_unique<FakeChannel>(/*id=*/1, /*remote_id=*/1, /*handle=*/2, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff_conn->unique_id());

  // Different handle + Different local id = Different unique id
  auto chan_diff =
      std::make_unique<FakeChannel>(/*id=*/1, /*remote_id=*/2, /*handle=*/2, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff->unique_id());
}

}  // namespace bt::l2cap::testing
