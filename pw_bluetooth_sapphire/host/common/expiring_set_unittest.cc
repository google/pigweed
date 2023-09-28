// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "expiring_set.h"

#include <gtest/gtest.h>
#include <pw_async/fake_dispatcher_fixture.h>

#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"

namespace bt {

namespace {

using ExpiringSetTest = pw::async::test::FakeDispatcherFixture;

TEST_F(ExpiringSetTest, Expiration) {
  ExpiringSet<std::string> set(dispatcher());

  set.add_until("Expired", now() - std::chrono::milliseconds(1));
  EXPECT_FALSE(set.contains("Expired"));

  set.add_until("Just a minute", now() + std::chrono::minutes(1));
  set.add_until("Two minutes", now() + std::chrono::minutes(2));
  EXPECT_TRUE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunFor(std::chrono::seconds(1));
  EXPECT_TRUE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunFor(std::chrono::minutes(1));
  EXPECT_FALSE(set.contains("Just a minute"));
  EXPECT_TRUE(set.contains("Two minutes"));

  RunFor(std::chrono::minutes(1));
  EXPECT_FALSE(set.contains("Just a minute"));
  EXPECT_FALSE(set.contains("Two minutes"));
}

TEST_F(ExpiringSetTest, Remove) {
  ExpiringSet<std::string> set(dispatcher());

  set.add_until("Expired", now() - std::chrono::milliseconds(1));
  EXPECT_FALSE(set.contains("Expired"));
  set.remove("Expired");

  set.add_until("Temporary", now() + std::chrono::seconds(1000));
  EXPECT_TRUE(set.contains("Temporary"));

  set.remove("Temporary");

  EXPECT_FALSE(set.contains("Temporary"));
}

}  // namespace

}  // namespace bt
