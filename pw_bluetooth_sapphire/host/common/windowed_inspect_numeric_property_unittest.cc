// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NINSPECT

#include "windowed_inspect_numeric_property.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/testing/inspect.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt {

namespace {

using namespace ::inspect::testing;

template <typename T>
class TestProperty {
 public:
  using ValueCallback = fit::function<void(const T& value)>;
  TestProperty() = default;
  TestProperty(T value, ValueCallback cb) : value_(value), value_cb_(std::move(cb)) {}

  void Add(const T& value) {
    value_ += value;
    if (value_cb_) {
      value_cb_(value_);
    }
  }

  void Subtract(const T& value) {
    value_ -= value;
    if (value_cb_) {
      value_cb_(value_);
    }
  }

 private:
  T value_;
  fit::function<void(const T& value)> value_cb_;
};

using WindowedProperty = WindowedInspectNumericProperty<TestProperty<int>, int>;
using WindowedInspectNumericPropertyTest = ::gtest::TestLoopFixture;

TEST_F(WindowedInspectNumericPropertyTest, AddTwoValues) {
  constexpr zx::duration kExpiryDuration = zx::min(3);
  WindowedProperty windowed_prop(kExpiryDuration);
  int value = 0;
  auto value_cb = [&](auto val) { value = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb));

  windowed_prop.Add(1);
  EXPECT_EQ(value, 1);
  RunLoopFor(zx::min(1));
  EXPECT_EQ(value, 1);

  windowed_prop.Add(2);
  EXPECT_EQ(value, 3);
  RunLoopFor(zx::min(1));
  EXPECT_EQ(value, 3);

  // Let first value expire.
  RunLoopFor(zx::min(1));
  EXPECT_EQ(value, 2);
  // Let second value expire.
  RunLoopFor(zx::min(1));
  EXPECT_EQ(value, 0);

  // Ensure timer doesn't fire again.
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value, 0);
}

TEST_F(WindowedInspectNumericPropertyTest, AddTwoValuesAtSameTime) {
  constexpr zx::duration kExpiryDuration = zx::min(3);
  WindowedProperty windowed_prop(kExpiryDuration);
  int value = 0;
  auto value_cb = [&](auto val) { value = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb));

  windowed_prop.Add(1);
  windowed_prop.Add(2);
  EXPECT_EQ(value, 3);
  RunLoopFor(zx::min(1));
  EXPECT_EQ(value, 3);
  RunLoopFor(zx::min(2));
  EXPECT_EQ(value, 0);

  // Ensure timer doesn't fire again.
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value, 0);
}

TEST_F(WindowedInspectNumericPropertyTest, AddValueThenExpireThenAddValue) {
  constexpr zx::duration kExpiryDuration = zx::min(3);
  WindowedProperty windowed_prop(kExpiryDuration);
  int value = 0;
  auto value_cb = [&](auto val) { value = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb));

  windowed_prop.Add(1);
  EXPECT_EQ(value, 1);
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value, 0);

  windowed_prop.Add(2);
  EXPECT_EQ(value, 2);
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value, 0);

  // Ensure timer doesn't fire again.
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value, 0);
}

TEST_F(WindowedInspectNumericPropertyTest,
       AddTwoValuesWithinResolutionIntervalExpiresBothSimultaneously) {
  constexpr zx::duration kExpiryDuration = zx::min(3);
  constexpr zx::duration kResolution = zx::sec(3);
  WindowedProperty windowed_prop(kExpiryDuration, kResolution);
  int value = 0;
  auto value_cb = [&](auto val) { value = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb));

  // First two values are within kResolution of each other in time.
  windowed_prop.Add(1);
  constexpr zx::duration kTinyDuration = zx::msec(1);
  RunLoopFor(kTinyDuration);
  windowed_prop.Add(1);
  EXPECT_EQ(value, 2);

  // Third value is spaced kResolution apart from the first value.
  RunLoopFor(kResolution - kTinyDuration);
  windowed_prop.Add(1);
  EXPECT_EQ(value, 3);

  // Let first value expire.
  RunLoopFor(kExpiryDuration - kResolution);

  // First and second values should have expired because they were merged.
  EXPECT_EQ(value, 1);

  // Let third value expire.
  RunLoopFor(kResolution);
  EXPECT_EQ(value, 0);
}

TEST_F(WindowedInspectNumericPropertyTest, SetPropertyClearsValueAndTimer) {
  constexpr zx::duration kExpiryDuration = zx::min(3);
  WindowedProperty windowed_prop(kExpiryDuration);
  int value_0 = 0;
  auto value_cb_0 = [&](auto val) { value_0 = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb_0));

  windowed_prop.Add(1);
  EXPECT_EQ(value_0, 1);
  int value_1 = 0;
  auto value_cb_1 = [&](auto val) { value_1 = val; };
  windowed_prop.SetProperty(TestProperty<int>(0, value_cb_1));
  // Ensure timer doesn't fire.
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value_0, 1);
  EXPECT_EQ(value_1, 0);

  // Ensure values can be added to new property.
  windowed_prop.Add(3);
  EXPECT_EQ(value_0, 1);
  EXPECT_EQ(value_1, 3);
  RunLoopFor(kExpiryDuration);
  EXPECT_EQ(value_0, 1);
  EXPECT_EQ(value_1, 0);
}

TEST_F(WindowedInspectNumericPropertyTest, AttachInspectRealIntProperty) {
  ::inspect::Inspector inspector;
  auto& root = inspector.GetRoot();

  constexpr zx::duration kExpiryDuration = zx::min(3);
  WindowedInspectIntProperty windowed_property(kExpiryDuration);
  windowed_property.AttachInspect(root, "windowed");

  auto hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(hierarchy.take_value(),
              AllOf(NodeMatches(PropertyList(ElementsAre(IntIs("windowed", 0))))));

  windowed_property.Add(7);

  hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(hierarchy.take_value(),
              AllOf(NodeMatches(PropertyList(ElementsAre(IntIs("windowed", 7))))));
}

}  // namespace

}  // namespace bt

#endif  // NINSPECT
