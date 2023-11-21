// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel.h"

namespace bt::l2cap::testing {

TEST(ChannelTest, UniqueId) {
  // Same handle + Same local id = Same unique id
  auto channel = std::make_unique<FakeChannel>(
      /*id=*/1, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  auto chan_diff_remote = std::make_unique<FakeChannel>(
      /*id=*/1, /*remote_id=*/2, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_EQ(channel->unique_id(), chan_diff_remote->unique_id());

  // Same handle + Different local id = Different unique id
  auto chan_diff_local = std::make_unique<FakeChannel>(
      /*id=*/2, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff_local->unique_id());

  // Same handle + Same local id = Same unique id
  auto chan_same = std::make_unique<FakeChannel>(
      /*id=*/1, /*remote_id=*/1, /*handle=*/1, bt::LinkType::kACL);
  ASSERT_EQ(channel->unique_id(), chan_same->unique_id());

  // Different handle + Same local id = Different unique id
  auto chan_diff_conn = std::make_unique<FakeChannel>(
      /*id=*/1, /*remote_id=*/1, /*handle=*/2, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff_conn->unique_id());

  // Different handle + Different local id = Different unique id
  auto chan_diff = std::make_unique<FakeChannel>(
      /*id=*/1, /*remote_id=*/2, /*handle=*/2, bt::LinkType::kACL);
  ASSERT_NE(channel->unique_id(), chan_diff->unique_id());
}

}  // namespace bt::l2cap::testing
