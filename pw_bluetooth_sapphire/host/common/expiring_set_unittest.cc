// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "expiring_set.h"

#include <gtest/gtest.h>
#include <pw_async_fuchsia/dispatcher.h>
#include <pw_async_fuchsia/util.h>

#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt {

namespace {

class ExpiringSetTest : public ::gtest::TestLoopFixture {
 public:
  zx::time now() { return pw_async_fuchsia::TimepointToZxTime(pw_dispatcher_.now()); }
  pw::async::Dispatcher& pw_dispatcher() { return pw_dispatcher_; }

 private:
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};
};

TEST_F(ExpiringSetTest, Expiration) {
  ExpiringSet<std::string> set(pw_dispatcher());

  set.add_until("Expired", now() - zx::msec(1));
  EXPECT_FALSE(set.contains("Expired"));

  set.add_until("Just a minute", now() + zx::min(1));
  set.add_until("Two minutes", now() + zx::min(2));
  EXPECT_TRUE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunLoopFor(zx::sec(1));
  EXPECT_TRUE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunLoopFor(zx::min(1));
  EXPECT_FALSE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunLoopFor(zx::min(1));
  EXPECT_FALSE(set.contains("Just a minute"));
  EXPECT_FALSE(set.contains("Two minutes"));
}

TEST_F(ExpiringSetTest, Remove) {
  ExpiringSet<std::string> set(pw_dispatcher());

  set.add_until("Expired", now() - zx::msec(1));
  EXPECT_FALSE(set.contains("Expired"));
  set.remove("Expired");

  set.add_until("Temporary", now() + zx::sec(1000));
  EXPECT_TRUE(set.contains("Temporary"));

  set.remove("Temporary");

  EXPECT_FALSE(set.contains("Temporary"));
}

}  // namespace

}  // namespace bt
