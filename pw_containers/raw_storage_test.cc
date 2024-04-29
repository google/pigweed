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

#include "pw_containers_private/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace pw::containers {
namespace {

using test::CopyOnly;
using test::Counter;
using test::MoveOnly;

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

TEST(RawStorage, Construct_CopyOnly) {
  internal::RawStorage<CopyOnly, 2> array;
  EXPECT_EQ(array.max_size(), 2u);
}

TEST(RawStorage, Construct_MoveOnly) {
  internal::RawStorage<MoveOnly, 2> array;
  EXPECT_EQ(array.max_size(), 2u);
}

TEST(RawStorage, Destruct) {
  Counter::Reset();

  {
    [[maybe_unused]] internal::RawStorage<Counter, 128> destroyed;
  }

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
