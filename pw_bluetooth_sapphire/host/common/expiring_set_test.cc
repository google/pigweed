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

#include "pw_bluetooth_sapphire/internal/host/common/expiring_set.h"

#include <gtest/gtest.h>
#include <pw_async/fake_dispatcher_fixture.h>

#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

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
