// Copyright 2020 The Pigweed Authors
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

#include "pw_containers/internal/raw_storage.h"

#include <array>
#include <cstddef>

#include "gtest/gtest.h"

namespace pw::containers {
namespace {

TEST(RawStorage, Construct_ZeroSized) {
  internal::RawStorage<int, 0> array;
  EXPECT_EQ(array.max_size(), 0u);
}

TEST(RawStorage, Construct_NonZeroSized) {
  internal::RawStorage<int, 3> array;
  EXPECT_EQ(array.max_size(), 3u);
}

TEST(RawStorage, Construct_Constexpr) {
  constexpr internal::RawStorage<int, 2> kArray;
  EXPECT_EQ(kArray.max_size(), 2u);
}
struct CopyOnly {
  explicit CopyOnly(int val) : value(val) {}

  CopyOnly(const CopyOnly& other) { value = other.value; }

  CopyOnly& operator=(const CopyOnly& other) {
    value = other.value;
    return *this;
  }

  CopyOnly(CopyOnly&&) = delete;

  int value;
};

TEST(RawStorage, Construct_CopyOnly) {
  internal::RawStorage<CopyOnly, 2> array;
  EXPECT_EQ(array.max_size(), 2u);
}

struct MoveOnly {
  explicit MoveOnly(int val) : value(val) {}

  MoveOnly(const MoveOnly&) = delete;

  MoveOnly(MoveOnly&& other) {
    value = other.value;
    other.value = kDeleted;
  }

  static constexpr int kDeleted = -1138;

  int value;
};

TEST(RawStorage, Construct_MoveOnly) {
  internal::RawStorage<MoveOnly, 2> array;
  EXPECT_EQ(array.max_size(), 2u);
}

struct Counter {
  static int created;
  static int destroyed;
  static int moved;

  static void Reset() { created = destroyed = moved = 0; }

  Counter() : value(0) { created += 1; }

  Counter(int val) : value(val) { created += 1; }

  Counter(const Counter& other) : value(other.value) { created += 1; }

  Counter(Counter&& other) : value(other.value) {
    other.value = 0;
    moved += 1;
  }

  Counter& operator=(const Counter& other) {
    value = other.value;
    created += 1;
    return *this;
  }

  Counter& operator=(Counter&& other) {
    value = other.value;
    other.value = 0;
    moved += 1;
    return *this;
  }

  ~Counter() { destroyed += 1; }

  int value;
};

int Counter::created = 0;
int Counter::destroyed = 0;
int Counter::moved = 0;

TEST(RawStorage, Destruct) {
  Counter::Reset();

  { [[maybe_unused]] internal::RawStorage<Counter, 128> destroyed; }

  EXPECT_EQ(Counter::created, 0);
  EXPECT_EQ(Counter::destroyed, 0);
}

static_assert(sizeof(internal::RawStorage<uint8_t, 42>) ==
              42 * sizeof(uint8_t));
static_assert(sizeof(internal::RawStorage<uint16_t, 42>) ==
              42 * sizeof(uint16_t));
static_assert(sizeof(internal::RawStorage<uint32_t, 42>) ==
              42 * sizeof(uint32_t));

}  // namespace
}  // namespace pw::containers
