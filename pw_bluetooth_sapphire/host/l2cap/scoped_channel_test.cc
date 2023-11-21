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

#include "pw_bluetooth_sapphire/internal/host/l2cap/scoped_channel.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel.h"

namespace bt::l2cap::testing {
namespace {

void DoNothing() {}
void NopRxCallback(ByteBufferPtr) {}

}  // namespace

TEST(ScopedChannelTest, Close) {
  auto chan = std::make_unique<FakeChannel>(1, 1, 1, bt::LinkType::kACL);
  ASSERT_TRUE(chan->Activate(NopRxCallback, DoNothing));
  ASSERT_TRUE(chan->activated());

  {
    ScopedChannel scoped(chan->GetWeakPtr());
    EXPECT_TRUE(chan->activated());
  }

  EXPECT_FALSE(chan->activated());
}

TEST(ScopedChannelTest, Reset) {
  auto chan1 = std::make_unique<FakeChannel>(1, 1, 1, bt::LinkType::kACL);
  auto chan2 = std::make_unique<FakeChannel>(1, 1, 1, bt::LinkType::kACL);
  ASSERT_TRUE(chan1->Activate(NopRxCallback, DoNothing));
  ASSERT_TRUE(chan2->Activate(NopRxCallback, DoNothing));
  ASSERT_TRUE(chan1->activated());
  ASSERT_TRUE(chan2->activated());

  ScopedChannel scoped(chan1->GetWeakPtr());
  EXPECT_TRUE(chan1->activated());

  scoped.Reset(chan2->GetWeakPtr());
  EXPECT_FALSE(chan1->activated());
  EXPECT_TRUE(chan2->activated());

  scoped.Reset(Channel::WeakPtr());
  EXPECT_FALSE(chan2->activated());
}

}  // namespace bt::l2cap::testing
