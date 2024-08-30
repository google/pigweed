// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/fuchsia/lib/fidl/hanging_getter.h"

#include <gtest/gtest.h>

namespace bt_lib_fidl {
namespace {

template <typename T, typename Getter = HangingGetter<T>>
class HangingGetterTestBase : public ::testing::Test {
 public:
  void Watch() {
    getter_.Watch([this](T value) {
      callback_count_++;
      last_value_ = value;
    });
  }

  int callback_count() const { return callback_count_; }
  const std::optional<T>& last_value() const { return last_value_; }

  Getter* getter() { return &getter_; }

 private:
  int callback_count_ = 0;
  std::optional<T> last_value_;
  Getter getter_;
};

using HangingGetterTest = HangingGetterTestBase<bool>;

TEST_F(HangingGetterTest, Armed) {
  EXPECT_FALSE(getter()->armed());
  Watch();
  EXPECT_TRUE(getter()->armed());
}

TEST_F(HangingGetterTest, WatchFailsWhilePending) {
  Watch();
  EXPECT_EQ(0, callback_count());
  Watch();
  EXPECT_EQ(0, callback_count());
}

TEST_F(HangingGetterTest, WatchCallbackDeferredWithoutAValue) {
  Watch();
  EXPECT_EQ(0, callback_count());
  EXPECT_FALSE(last_value().has_value());

  getter()->Set(false);
  EXPECT_EQ(1, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_FALSE(*last_value());
}

TEST_F(HangingGetterTest, WatchCallbackRunsRightAwayWithAValue) {
  getter()->Set(false);
  EXPECT_EQ(0, callback_count());
  EXPECT_FALSE(last_value().has_value());

  // Assign the value again to test that the latest value is returned in
  // Watch().
  getter()->Set(true);
  EXPECT_EQ(0, callback_count());
  EXPECT_FALSE(last_value().has_value());
  EXPECT_FALSE(getter()->armed());

  Watch();
  EXPECT_EQ(1, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_TRUE(*last_value());

  // Calling Watch() again should succeed and defer the callback.
  Watch();
  EXPECT_EQ(1, callback_count());
}

TEST_F(HangingGetterTest, MultipleWatchersPending) {
  Watch();  // 1
  Watch();  // 2
  Watch();  // 3
  getter()->Set(true);
  EXPECT_EQ(3, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_TRUE(*last_value());
  EXPECT_FALSE(getter()->armed());
}

TEST_F(HangingGetterTest, OnlyFirstOfManyWatchersRunsWithAValue) {
  getter()->Set(true);

  // Only the first watch call should result in a callback. The following two
  // are expected to remain pending until a new value gets assigned.
  Watch();  // 1
  Watch();  // 2
  Watch();  // 3
  EXPECT_EQ(1, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_TRUE(*last_value());

  EXPECT_TRUE(getter()->armed());
  getter()->Set(false);
  EXPECT_EQ(3, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_FALSE(*last_value());
}

TEST_F(HangingGetterTest, WatchClearsExistingValue) {
  getter()->Set(true);
  Watch();
  EXPECT_EQ(1, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_TRUE(*last_value());

  // Callback should be deferred.
  Watch();
  EXPECT_EQ(1, callback_count());

  // Test the deferral.
  getter()->Set(true);
  EXPECT_EQ(2, callback_count());
}

TEST_F(HangingGetterTest, Transform) {
  getter()->Set(false);
  getter()->Transform([](bool current) {
    EXPECT_FALSE(current);
    return true;
  });

  Watch();
  EXPECT_EQ(1, callback_count());
  ASSERT_TRUE(last_value().has_value());
  EXPECT_TRUE(*last_value());
}

using HangingVectorGetterTest =
    HangingGetterTestBase<std::vector<bool>, HangingVectorGetter<bool>>;

TEST_F(HangingVectorGetterTest, AddAndWatch) {
  getter()->Add(false);
  getter()->Add(true);

  Watch();
  EXPECT_EQ(1, callback_count());
  EXPECT_TRUE(last_value().has_value());
  EXPECT_EQ(2u, last_value()->size());
  EXPECT_FALSE((*last_value())[0]);
  EXPECT_TRUE((*last_value())[1]);
}

TEST_F(HangingVectorGetterTest, WatchAndAdd) {
  Watch();
  EXPECT_EQ(0, callback_count());

  getter()->Add(true);
  EXPECT_EQ(1, callback_count());
  EXPECT_TRUE(last_value().has_value());
  EXPECT_EQ(1u, last_value()->size());
  EXPECT_TRUE((*last_value())[0]);
}

}  // namespace
}  // namespace bt_lib_fidl
