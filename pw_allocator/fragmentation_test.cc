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

#include "pw_allocator/fragmentation.h"

#include "pw_unit_test/framework.h"

namespace {

using ::pw::allocator::Fragmentation;

TEST(FragmentationTest, ValuesAreCorrect) {
  Fragmentation fragmentation;
  fragmentation.AddFragment(867);
  fragmentation.AddFragment(5309);
  EXPECT_EQ(fragmentation.sum_of_squares.hi, 0U);
  EXPECT_EQ(fragmentation.sum_of_squares.lo, 867U * 867U + 5309U * 5309U);
  EXPECT_EQ(fragmentation.sum, 867U + 5309U);
}

TEST(FragmentationTest, HandlesOverflow) {
  constexpr size_t kHalfWord = size_t(1) << sizeof(size_t) * 4;
  Fragmentation fragmentation;
  fragmentation.AddFragment(kHalfWord);
  fragmentation.AddFragment(kHalfWord);
  fragmentation.AddFragment(kHalfWord);
  fragmentation.AddFragment(kHalfWord);
  EXPECT_EQ(fragmentation.sum_of_squares.hi, 4U);
  EXPECT_EQ(fragmentation.sum_of_squares.lo, 0U);
  EXPECT_EQ(fragmentation.sum, 4 * kHalfWord);
}

}  // namespace
