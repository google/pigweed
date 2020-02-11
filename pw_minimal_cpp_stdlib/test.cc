// Copyright 2019 The Pigweed Authors
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

// Include all of the provided headers, even if they aren't tested.
#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"

namespace {

TEST(Algorithm, Basic) {
  static_assert(std::min(1, 2) == 1);
  static_assert(std::max(1, 2) == 2);

  EXPECT_EQ(std::forward<int>(2), 2);
}

TEST(Array, Basic) {
  constexpr std::array<int, 4> array{0, 1, 2, 3};

  static_assert(array[2] == 2);

  for (int i = 0; i < static_cast<int>(array.size()); ++i) {
    EXPECT_EQ(i, array[i]);
  }
}

TEST(Cmath, Basic) {
  EXPECT_EQ(std::abs(-1), 1);
  EXPECT_EQ(std::abs(1), 1);

  EXPECT_TRUE(std::isfinite(1.0));
  EXPECT_FALSE(std::isfinite(1.0 / 0.0));

  EXPECT_FALSE(std::isnan(1.0));
  EXPECT_TRUE(std::isnan(0.0 / 0.0));

  EXPECT_FALSE(std::signbit(1.0));
  EXPECT_TRUE(std::signbit(-1.0));
}

TEST(Cstddef, Basic) {
  using std::byte;
  byte foo = byte{12};
  EXPECT_EQ(foo, byte{12});
}

TEST(Iterator, Basic) {
  std::array<int, 3> foo{3, 2, 1};

  EXPECT_EQ(std::data(foo), foo.data());
  EXPECT_EQ(std::size(foo), foo.size());

  foo.fill(99);
  EXPECT_EQ(foo[0], 99);
  EXPECT_EQ(foo[1], 99);
  EXPECT_EQ(foo[2], 99);
}

template <typename T>
int SumFromInitializerList(std::initializer_list<T> values) {
  int sum = 0;
  for (auto value : values) {
    sum += value;
  }
  return sum;
}
TEST(InitializerList, Empty) {
  std::initializer_list<int> mt;
  EXPECT_EQ(0, SumFromInitializerList(mt));

  EXPECT_EQ(0, SumFromInitializerList<float>({}));
}

TEST(InitializerList, Declared) {
  std::initializer_list<char> list{'\3', '\3', '\4'};
  EXPECT_EQ(10, SumFromInitializerList(list));
}

TEST(InitializerList, Inline) {
  EXPECT_EQ(42, SumFromInitializerList<long>({42}));
  EXPECT_EQ(2, SumFromInitializerList<bool>({true, false, true}));
  EXPECT_EQ(15, SumFromInitializerList({1, 2, 3, 4, 5}));
}

TEST(Limits, Basic) {
  static_assert(std::numeric_limits<unsigned char>::is_specialized);
  static_assert(std::numeric_limits<unsigned char>::is_integer);
  static_assert(std::numeric_limits<unsigned char>::min() == 0u);
  static_assert(std::numeric_limits<unsigned char>::max() == 255u);

  static_assert(std::numeric_limits<signed char>::is_specialized);
  static_assert(std::numeric_limits<signed char>::is_integer);
  static_assert(std::numeric_limits<signed char>::min() == -128);
  static_assert(std::numeric_limits<signed char>::max() == 127);

  // Assume 64-bit long long
  static_assert(std::numeric_limits<long long>::is_specialized);
  static_assert(std::numeric_limits<long long>::is_integer);
  static_assert(std::numeric_limits<long long>::min() ==
                (-9223372036854775807ll - 1));
  static_assert(std::numeric_limits<long long>::max() == 9223372036854775807ll);

  static_assert(std::numeric_limits<unsigned long long>::is_specialized);
  static_assert(std::numeric_limits<unsigned long long>::is_integer);
  static_assert(std::numeric_limits<unsigned long long>::min() == 0u);
  static_assert(std::numeric_limits<unsigned long long>::max() ==
                18446744073709551615ull);
}

TEST(New, PlacementNew) {
  unsigned char value[4];
  new (value) int(1234);

  int int_value;
  std::memcpy(&int_value, value, sizeof(int_value));
  EXPECT_EQ(1234, int_value);
}

TEST(New, Launder) {
  unsigned char value[4];
  int* int_ptr = std::launder(reinterpret_cast<int*>(value));
  EXPECT_EQ(static_cast<void*>(int_ptr), static_cast<void*>(value));
}

TEST(StringView, Basic) {
  constexpr std::string_view value("1234567890");
  static_assert(value.size() == 10);
  static_assert(value[1] == '2');

  char buffer[] = "!!!!!";
  constexpr size_t buffer_size = sizeof(buffer) - 1;  // always keep the \0

  value.copy(buffer, buffer_size, 10);
  EXPECT_STREQ(buffer, "!!!!!");

  value.copy(buffer, buffer_size, 9);
  EXPECT_STREQ(buffer, "0!!!!");

  value.copy(buffer, buffer_size, 2);
  EXPECT_STREQ(buffer, "34567");

  value.copy(buffer, buffer_size);
  EXPECT_STREQ(buffer, "12345");
}

TEST(TypeTraits, Basic) {
  static_assert(std::is_integral_v<bool>);
  static_assert(!std::is_integral_v<float>);

  static_assert(std::is_floating_point_v<float>);
  static_assert(!std::is_floating_point_v<bool>);

  static_assert(std::is_same_v<float, float>);
  static_assert(!std::is_same_v<char, unsigned char>);
}

struct MoveTester {
  MoveTester(int magic_value) : magic_value(magic_value), moved(false) {}

  MoveTester(const MoveTester&) = default;

  MoveTester(MoveTester&& other) : magic_value(other.magic_value), moved(true) {
    other.magic_value = 0xffff;
  }

  int magic_value;
  bool moved;
};

TEST(Utility, Move) {
  MoveTester test(123);

  MoveTester copied(test);
  EXPECT_EQ(copied.magic_value, 123);
  EXPECT_FALSE(copied.moved);

  MoveTester moved(std::move(copied));
  EXPECT_EQ(123, moved.magic_value);
  EXPECT_EQ(0xffff, copied.magic_value);
  EXPECT_TRUE(moved.moved);
}

}  // namespace
