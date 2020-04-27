// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel.h"

#include <gtest/gtest.h>

#include "fake_channel.h"

namespace bt {
namespace l2cap {
namespace testing {

TEST(L2CAP_ChannelTest, UniqueId) {
  // Same channel and handle on the local side produces the same unique id
  auto chan = fbl::AdoptRef(new FakeChannel(1, 1, 1, hci::Connection::LinkType::kACL));
  auto chan_diffremote = fbl::AdoptRef(new FakeChannel(1, 2, 1, hci::Connection::LinkType::kACL));

  ASSERT_EQ(chan->unique_id(), chan_diffremote->unique_id());

  // Different handle, same local id produces different unique ids
  auto chan_diffconn = fbl::AdoptRef(new FakeChannel(1, 1, 2, hci::Connection::LinkType::kACL));

  ASSERT_NE(chan->unique_id(), chan_diffconn->unique_id());

  // Same handle, different local id produces different unique ids.
  auto chan_difflocalid = fbl::AdoptRef(new FakeChannel(2, 1, 1, hci::Connection::LinkType::kACL));

  ASSERT_NE(chan->unique_id(), chan_difflocalid->unique_id());

  // Same everything produces same unique ids.
  auto chan_stillsame = fbl::AdoptRef(new FakeChannel(1, 1, 1, hci::Connection::LinkType::kACL));

  ASSERT_EQ(chan->unique_id(), chan_stillsame->unique_id());
}

}  // namespace testing
}  // namespace l2cap
}  // namespace bt
