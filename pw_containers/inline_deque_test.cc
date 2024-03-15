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

#include "pw_containers/inline_deque.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_containers/algorithm.h"
#include "pw_containers_private/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace pw::containers {
namespace {

using namespace std::literals::string_view_literals;
using test::CopyOnly;
using test::Counter;
using test::MoveOnly;

TEST(InlineDeque, Construct_Sized) {
  InlineDeque<int, 3> deque;
  EXPECT_TRUE(deque.empty());
  EXPECT_EQ(deque.size(), 0u);
  EXPECT_EQ(deque.max_size(), 3u);
}

TEST(InlineDeque, Construct_GenericSized) {
  InlineDeque<int, 3> sized_deque;
  InlineDeque<int>& deque(sized_deque);
  EXPECT_TRUE(deque.empty());
  EXPECT_EQ(deque.size(), 0u);
  EXPECT_EQ(deque.max_size(), 3u);
}

TEST(InlineDeque, Construct_CopySameCapacity) {
  InlineDeque<CopyOnly, 4> deque(4, CopyOnly(123));
  const auto& deque_ref = deque;
  InlineDeque<CopyOnly, 4> copied(deque_ref);

  ASSERT_EQ(4u, deque.size());
  EXPECT_EQ(123, deque[3].value);

  ASSERT_EQ(4u, copied.size());
  EXPECT_EQ(123, copied[3].value);
}

TEST(InlineDeque, Construct_MoveSameCapacity) {
  InlineDeque<MoveOnly, 4> deque;
  deque.emplace_back(MoveOnly(1));
  deque.emplace_back(MoveOnly(2));
  deque.emplace_back(MoveOnly(3));
  deque.emplace_back(MoveOnly(4));
  InlineDeque<MoveOnly, 4> moved(std::move(deque));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, deque.size());

  ASSERT_EQ(4u, moved.size());
  EXPECT_EQ(4, moved[3].value);
}

TEST(InlineDeque, Construct_CopyLargerCapacity) {
  InlineDeque<CopyOnly, 4> deque(4, CopyOnly(123));
  InlineDeque<CopyOnly, 5> copied(deque);

  ASSERT_EQ(4u, deque.size());
  EXPECT_EQ(123, deque[3].value);

  ASSERT_EQ(4u, copied.size());
  EXPECT_EQ(123, copied[3].value);
}

TEST(InlineDeque, Construct_MoveLargerCapacity) {
  InlineDeque<MoveOnly, 4> deque;
  deque.emplace_back(MoveOnly(1));
  deque.emplace_back(MoveOnly(2));
  deque.emplace_back(MoveOnly(3));
  deque.emplace_back(MoveOnly(4));
  InlineDeque<MoveOnly, 5> moved(std::move(deque));

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, deque.size());

  ASSERT_EQ(4u, moved.size());
  EXPECT_EQ(4, moved[3].value);
}

TEST(InlineDeque, Construct_CopySmallerCapacity) {
  InlineDeque<CopyOnly, 4> deque(3, CopyOnly(123));
  InlineDeque<CopyOnly, 3> copied(deque);

  ASSERT_EQ(3u, deque.size());
  EXPECT_EQ(123, deque[2].value);

  ASSERT_EQ(3u, copied.size());
  EXPECT_EQ(123, copied[2].value);
}

TEST(InlineDeque, Destruct_ZeroLength) {
  Counter::Reset();
  {
    InlineDeque<Counter, 0> deque;
    EXPECT_EQ(deque.size(), 0u);
  }
  EXPECT_EQ(Counter::created, 0);
  EXPECT_EQ(Counter::destroyed, 0);
}

TEST(InlineDeque, Destruct_Empty) {
  Counter::Reset();

  {
    InlineDeque<Counter, 3> deque;
    EXPECT_EQ(deque.size(), 0u);
  }
  EXPECT_EQ(Counter::created, 0);
  EXPECT_EQ(Counter::destroyed, 0);
}

TEST(InlineDeque, Destruct_MultipleEntries) {
  Counter value;
  Counter::Reset();

  { InlineDeque<Counter, 128> deque(100, value); }

  EXPECT_EQ(Counter::created, 100);
  EXPECT_EQ(Counter::destroyed, 100);
}

TEST(InlineDeque, Assign_InitializerList) {
  InlineDeque<int, 4> deque = {1, 3, 5, 7};

  ASSERT_EQ(4u, deque.size());

  EXPECT_EQ(1, deque[0]);
  EXPECT_EQ(3, deque[1]);
  EXPECT_EQ(5, deque[2]);
  EXPECT_EQ(7, deque[3]);
}

TEST(InlineDeque, Assign_CopySameCapacity) {
  InlineDeque<CopyOnly, 4> deque(4, CopyOnly(123));
  InlineDeque<CopyOnly, 4> copied = deque;

  ASSERT_EQ(4u, deque.size());
  EXPECT_EQ(123, deque[3].value);

  ASSERT_EQ(4u, copied.size());
  EXPECT_EQ(123, copied[3].value);
}

TEST(InlineDeque, Assign_CopyLargerCapacity) {
  InlineDeque<CopyOnly, 4> deque(4, CopyOnly(123));
  InlineDeque<CopyOnly, 5> copied = deque;

  ASSERT_EQ(4u, deque.size());
  EXPECT_EQ(123, deque[3].value);

  ASSERT_EQ(4u, copied.size());
  EXPECT_EQ(123, copied[3].value);
}

TEST(InlineDeque, Assign_CopySmallerCapacity) {
  InlineDeque<CopyOnly, 4> deque(3, CopyOnly(123));
  InlineDeque<CopyOnly, 3> copied = deque;

  ASSERT_EQ(3u, deque.size());
  EXPECT_EQ(123, deque[2].value);

  ASSERT_EQ(3u, copied.size());
  EXPECT_EQ(123, copied[2].value);
}

TEST(InlineDeque, Assign_MoveSameCapacity) {
  InlineDeque<MoveOnly, 4> deque;
  deque.emplace_back(MoveOnly(1));
  deque.emplace_back(MoveOnly(2));
  deque.emplace_back(MoveOnly(3));
  deque.emplace_back(MoveOnly(4));
  InlineDeque<MoveOnly, 4> moved = std::move(deque);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, deque.size());

  ASSERT_EQ(4u, moved.size());
  EXPECT_EQ(4, moved[3].value);
}

TEST(InlineDeque, Assign_MoveLargerCapacity) {
  InlineDeque<MoveOnly, 4> deque;
  deque.emplace_back(MoveOnly(1));
  deque.emplace_back(MoveOnly(2));
  deque.emplace_back(MoveOnly(3));
  deque.emplace_back(MoveOnly(4));
  InlineDeque<MoveOnly, 5> moved = std::move(deque);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, deque.size());

  ASSERT_EQ(4u, moved.size());
  EXPECT_EQ(4, moved[3].value);
}

TEST(InlineDeque, Assign_MoveSmallerCapacity) {
  InlineDeque<MoveOnly, 4> deque;
  deque.emplace_back(MoveOnly(1));
  deque.emplace_back(MoveOnly(2));
  deque.emplace_back(MoveOnly(3));
  InlineDeque<MoveOnly, 3> moved = std::move(deque);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, deque.size());

  ASSERT_EQ(3u, moved.size());
  EXPECT_EQ(3, moved[2].value);
}

TEST(InlineDeque, Access_Iterator) {
  InlineDeque<Counter, 2> deque(2);
  for (Counter& item : deque) {
    EXPECT_EQ(item.value, 0);
  }
  for (const Counter& item : deque) {
    EXPECT_EQ(item.value, 0);
  }
}

TEST(InlineDeque, Access_ConstIterator) {
  const InlineDeque<Counter, 2> deque(2);
  for (const Counter& item : deque) {
    EXPECT_EQ(item.value, 0);
  }
}

TEST(InlineDeque, Access_ZeroLength) {
  InlineDeque<Counter, 0> deque;

  EXPECT_EQ(0u, deque.size());
  EXPECT_EQ(0u, deque.max_size());
  EXPECT_TRUE(deque.empty());
  EXPECT_TRUE(deque.full());

  for (Counter& item : deque) {
    (void)item;
    FAIL();
  }
}

TEST(InlineDeque, Access_ContiguousData) {
  // Content = {}, Storage = [x, x]
  InlineDeque<int, 2> deque;

  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_EQ(first.size(), 0u);
    EXPECT_EQ(second.size(), 0u);
  }

  // Content = {1}, Storage = [1, x]
  deque.push_back(1);
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 1>{1}));
    EXPECT_TRUE(Equal(second, std::array<int, 0>{}));
  }

  // Content = {1, 2}, Storage = [1, 2]
  deque.push_back(2);
  EXPECT_TRUE(deque.full());
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 2>{1, 2}));
    EXPECT_TRUE(Equal(second, std::array<int, 0>{}));
  }

  // Content = {2}, Storage = [x, 2]
  deque.pop_front();
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 1>{2}));
    EXPECT_TRUE(Equal(second, std::array<int, 0>{}));
  }

  // Content = {2, 1}, Storage = [1, 2]
  deque.push_back(1);
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 1>{2}));
    EXPECT_TRUE(Equal(second, std::array<int, 1>{1}));
  }

  // Content = {1}, Storage = [1, x]
  deque.pop_front();
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 1>{1}));
    EXPECT_TRUE(Equal(second, std::array<int, 0>{}));
  }

  // Content = {1, 2}, Storage = [1, 2]
  deque.push_back(2);
  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_TRUE(Equal(first, std::array<int, 2>{1, 2}));
    EXPECT_TRUE(Equal(second, std::array<int, 0>{}));
  }
}

TEST(InlineDeque, Access_ConstContiguousData) {
  // Content = {1, 2}, Storage = [1, 2]
  const InlineDeque<int, 2> deque = {1, 2};

  {
    auto [first, second] = deque.contiguous_data();
    EXPECT_EQ(first.size(), 2u);
    EXPECT_EQ(second.size(), 0u);
  }
}

TEST(InlineDeque, Modify_Clear) {
  Counter::Reset();

  InlineDeque<Counter, 100> deque;
  deque.emplace_back();
  deque.emplace_back();
  deque.emplace_back();

  deque.clear();

  EXPECT_EQ(3, Counter::created);
  EXPECT_EQ(3, Counter::destroyed);
}

TEST(InlineDeque, Modify_PushBack_Copy) {
  Counter value(99);
  Counter::Reset();

  {
    InlineDeque<Counter, 10> deque;
    deque.push_back(value);

    ASSERT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 99);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(InlineDeque, Modify_PushBack_Move) {
  Counter::Reset();

  {
    Counter value(99);
    InlineDeque<Counter, 10> deque;
    deque.push_back(std::move(value));

    EXPECT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 99);
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(value.value, 0);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 2);
  EXPECT_EQ(Counter::moved, 1);
}

TEST(InlineDeque, Modify_EmplaceBack) {
  Counter::Reset();

  {
    InlineDeque<Counter, 10> deque;
    deque.emplace_back(314);

    ASSERT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 314);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(InlineDeque, Modify_WrapForwards) {
  Counter::Reset();

  {
    InlineDeque<Counter, 3> deque;
    deque.emplace_back(1);
    deque.emplace_back(2);
    deque.emplace_back(3);

    ASSERT_EQ(deque.size(), 3u);
    EXPECT_EQ(deque[0].value, 1);
    EXPECT_EQ(deque.front().value, 1);
    EXPECT_EQ(deque[1].value, 2);
    EXPECT_EQ(deque[2].value, 3);
    EXPECT_EQ(deque.back().value, 3);

    deque.pop_front();
    deque.emplace_back(4);

    ASSERT_EQ(deque.size(), 3u);
    EXPECT_EQ(deque[0].value, 2);
    EXPECT_EQ(deque.front().value, 2);
    EXPECT_EQ(deque[1].value, 3);
    EXPECT_EQ(deque[2].value, 4);
    EXPECT_EQ(deque.back().value, 4);
  }

  EXPECT_EQ(Counter::created, 4);
  EXPECT_EQ(Counter::destroyed, 4);
}

TEST(InlineDeque, Modify_WrapBackwards) {
  Counter::Reset();

  {
    InlineDeque<Counter, 3> deque;
    deque.emplace_front(1);
    deque.emplace_front(2);
    deque.emplace_front(3);

    ASSERT_EQ(deque.size(), 3u);
    EXPECT_EQ(deque[0].value, 3);
    EXPECT_EQ(deque.front().value, 3);
    EXPECT_EQ(deque[1].value, 2);
    EXPECT_EQ(deque[2].value, 1);
    EXPECT_EQ(deque.back().value, 1);

    deque.pop_back();
    deque.emplace_front(4);

    ASSERT_EQ(deque.size(), 3u);
    EXPECT_EQ(deque[0].value, 4);
    EXPECT_EQ(deque.front().value, 4);
    EXPECT_EQ(deque[1].value, 3);
    EXPECT_EQ(deque[2].value, 2);
    EXPECT_EQ(deque.back().value, 2);
  }

  EXPECT_EQ(Counter::created, 4);
  EXPECT_EQ(Counter::destroyed, 4);
}

TEST(InlineDeque, Modify_PushFront_Copy) {
  Counter value(99);
  Counter::Reset();

  {
    InlineDeque<Counter, 10> deque;
    deque.push_front(value);

    EXPECT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 99);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(InlineDeque, Modify_PushFront_Move) {
  Counter::Reset();

  {
    Counter value(99);
    InlineDeque<Counter, 10> deque;
    deque.push_front(std::move(value));

    EXPECT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 99);
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(value.value, 0);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 2);
  EXPECT_EQ(Counter::moved, 1);
}

TEST(InlineDeque, Modify_EmplaceFront) {
  Counter::Reset();

  {
    InlineDeque<Counter, 10> deque;
    deque.emplace_front(314);

    EXPECT_EQ(deque.size(), 1u);
    EXPECT_EQ(deque.front().value, 314);
  }

  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(InlineDeque, Modify_PopBack) {
  Counter::Reset();

  InlineDeque<Counter, 3> deque;
  deque.emplace_front(1);  // This wraps to the other end.
  deque.emplace_back(2);   // This is the first entry in storage.
  deque.emplace_back(3);
  // Content = {1, 2, 3}, Storage = [2, 3, 1]

  ASSERT_EQ(deque.size(), 3u);
  EXPECT_EQ(deque[0].value, 1);
  EXPECT_EQ(deque[1].value, 2);
  EXPECT_EQ(deque[2].value, 3);

  deque.pop_back();
  // Content = {1, 2}, Storage = [2, x, 1]
  ASSERT_EQ(deque.size(), 2u);
  EXPECT_EQ(deque[0].value, 1);
  EXPECT_EQ(deque[1].value, 2);

  // This wraps around.
  deque.pop_back();
  // Content = {1}, Storage = [x, x, 1]

  ASSERT_EQ(deque.size(), 1u);
  EXPECT_EQ(deque[0].value, 1);

  EXPECT_EQ(Counter::created, 3);
  EXPECT_EQ(Counter::destroyed, 2);
}

TEST(InlineDeque, Modify_PopFront) {
  Counter::Reset();

  InlineDeque<Counter, 3> deque;
  deque.emplace_front(1);  // This wraps to the other end.
  deque.emplace_back(2);   // This is the first entry in storage.
  deque.emplace_back(3);
  // Content = {1, 2, 3}, Storage = [2, 3, 1]

  ASSERT_EQ(deque.size(), 3u);
  EXPECT_EQ(deque[0].value, 1);
  EXPECT_EQ(deque[1].value, 2);
  EXPECT_EQ(deque[2].value, 3);

  // This wraps around
  deque.pop_front();
  // Content = {2, 3}, Storage = [2, 3, x]

  EXPECT_EQ(deque.size(), 2u);
  EXPECT_EQ(deque[0].value, 2);
  EXPECT_EQ(deque[1].value, 3);

  deque.pop_front();
  // Content = {3}, Storage = [x, 3, x]
  ASSERT_EQ(deque.size(), 1u);
  EXPECT_EQ(deque[0].value, 3);

  EXPECT_EQ(Counter::created, 3);
  EXPECT_EQ(Counter::destroyed, 2);
}

TEST(InlineDeque, Modify_Resize_Larger) {
  InlineDeque<CopyOnly, 10> deque(1, CopyOnly(123));
  deque.resize(3, CopyOnly(123));

  EXPECT_EQ(deque.size(), 3u);
  for (auto& i : deque) {
    EXPECT_EQ(i.value, 123);
  }
}

TEST(InlineDeque, Modify_Resize_LargerThanMax) {
  InlineDeque<CopyOnly, 10> deque;
  deque.resize(1000, CopyOnly(123));

  EXPECT_EQ(deque.size(), 10u);
  for (auto& i : deque) {
    EXPECT_EQ(i.value, 123);
  }
}

TEST(InlineDeque, Modify_Resize_Smaller) {
  InlineDeque<CopyOnly, 10> deque(9, CopyOnly(123));
  deque.resize(3, CopyOnly(123));

  EXPECT_EQ(deque.size(), 3u);
  for (auto& i : deque) {
    EXPECT_EQ(i.value, 123);
  }
}

TEST(InlineDeque, Modify_Resize_Zero) {
  InlineDeque<CopyOnly, 10> deque(10, CopyOnly(123));
  deque.resize(0, CopyOnly(123));

  EXPECT_EQ(deque.size(), 0u);
}

TEST(InlineDeque, Generic) {
  InlineDeque<int, 10> deque;
  InlineDeque<int>& generic_deque(deque);
  generic_deque = {1, 2, 3, 4, 5};

  EXPECT_EQ(generic_deque.size(), deque.size());
  EXPECT_EQ(generic_deque.max_size(), deque.max_size());

  unsigned short i = 0;
  for (int value : deque) {
    EXPECT_EQ(value, generic_deque[i]);
    i += 1;
  }

  i = 0;
  for (int value : generic_deque) {
    EXPECT_EQ(deque[i], value);
    i += 1;
  }
}

TEST(InlineDeque, ConstexprMaxSize) {
  InlineDeque<int, 10> deque;
  constexpr size_t kMaxSize = deque.max_size();
  EXPECT_EQ(deque.max_size(), kMaxSize);

  // Ensure the generic sized container does not have a constexpr max_size().
  [[maybe_unused]] InlineDeque<int>& generic_deque(deque);
#if PW_NC_TEST(InlineDeque_GenericMaxSize_NotConstexpr)
  PW_NC_EXPECT_CLANG(
      "kGenericMaxSize.* must be initialized by a constant expression");
  PW_NC_EXPECT_GCC("call to non-'constexpr' function .*InlineDeque.*max_size");
  [[maybe_unused]] constexpr size_t kGenericMaxSize = generic_deque.max_size();
#endif  // PW_NC_TEST
}

TEST(InlineDeque, StdMaxElement) {
  // Content = {1, 2, 3, 4}, Storage = [1, 2, 3, 4]
  InlineDeque<int, 4> deque = {1, 2, 3, 4};

  auto max_element_it = std::max_element(deque.begin(), deque.end());
  ASSERT_NE(max_element_it, deque.end());
  EXPECT_EQ(*max_element_it, 4);

  // Content = {2, 3, 4}, Storage = [x, 2, 3, 4]
  deque.pop_front();

  max_element_it = std::max_element(deque.begin(), deque.end());
  ASSERT_NE(max_element_it, deque.end());
  EXPECT_EQ(*max_element_it, 4);

  // Content = {2, 3, 4, 5}, Storage = [5, 2, 3, 4]
  deque.push_back(5);
  max_element_it = std::max_element(deque.begin(), deque.end());
  ASSERT_NE(max_element_it, deque.end());
  EXPECT_EQ(*max_element_it, 5);

  // Content = {}, Storage = [x, x, x, x]
  deque.clear();

  max_element_it = std::max_element(deque.begin(), deque.end());
  ASSERT_EQ(max_element_it, deque.end());
}

TEST(InlineDeque, StdMaxElementConst) {
  // Content = {1, 2, 3, 4}, Storage = [1, 2, 3, 4]
  InlineDeque<int, 4> deque = {1, 2, 3, 4};

  auto max_element_it = std::max_element(deque.cbegin(), deque.cend());
  ASSERT_NE(max_element_it, deque.cend());
  EXPECT_EQ(*max_element_it, 4);

  // Content = {2, 3, 4}, Storage = [x, 2, 3, 4]
  deque.pop_front();

  max_element_it = std::max_element(deque.cbegin(), deque.cend());
  ASSERT_NE(max_element_it, deque.cend());
  EXPECT_EQ(*max_element_it, 4);

  // Content = {2, 3, 4, 5}, Storage = [5, 2, 3, 4]
  deque.push_back(5);
  max_element_it = std::max_element(deque.cbegin(), deque.cend());
  ASSERT_NE(max_element_it, deque.cend());
  EXPECT_EQ(*max_element_it, 5);

  // Content = {}, Storage = [x, x, x, x]
  deque.clear();

  max_element_it = std::max_element(deque.cbegin(), deque.cend());
  ASSERT_EQ(max_element_it, deque.cend());
}

TEST(InlineDeque, OperatorPlus) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(*(deque.begin() + i), static_cast<int>(i + 1));
    ASSERT_EQ(*(i + deque.begin()), static_cast<int>(i + 1));
  }

  ASSERT_EQ(deque.begin() + deque.size(), deque.end());
}

TEST(InlineDeque, OperatorPlusPlus) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  auto it = deque.begin();

  ASSERT_EQ(*it, 1);
  it++;
  ASSERT_EQ(*it, 2);
  it++;
  ASSERT_EQ(*it, 3);
  it++;
  ASSERT_EQ(*it, 4);
  it++;

  ASSERT_EQ(it, deque.end());
}

TEST(InlineDeque, OperatorPlusEquals) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  auto it = deque.begin();

  ASSERT_EQ(*it, 1);
  it += 1;
  ASSERT_EQ(*it, 2);
  it += 1;
  ASSERT_EQ(*it, 3);
  it += 1;
  ASSERT_EQ(*it, 4);
  it += 1;
  ASSERT_EQ(it, deque.end());

  it = deque.begin();
  ASSERT_EQ(*it, 1);
  it += 2;
  ASSERT_EQ(*it, 3);
  it += 2;
  ASSERT_EQ(it, deque.end());

  it = deque.begin();
  it += deque.size();

  ASSERT_EQ(it, deque.end());
}

TEST(InlineDeque, OpeartorMinus) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 1; i <= 4; i++) {
    ASSERT_EQ(*(deque.end() - i), static_cast<int>(5 - i));
  }

  ASSERT_EQ(deque.end() - deque.size(), deque.begin());
}
TEST(InlineDeque, OperatorMinusMinus) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  auto it = deque.end();

  it--;
  ASSERT_EQ(*it, 4);
  it--;
  ASSERT_EQ(*it, 3);
  it--;
  ASSERT_EQ(*it, 2);
  it--;
  ASSERT_EQ(*it, 1);

  ASSERT_EQ(it, deque.begin());
}

TEST(InlineDeque, OperatorMinusEquals) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  auto it = deque.end();

  it -= 1;
  ASSERT_EQ(*it, 4);
  it -= 1;
  ASSERT_EQ(*it, 3);
  it -= 1;
  ASSERT_EQ(*it, 2);
  it -= 1;
  ASSERT_EQ(*it, 1);

  ASSERT_EQ(it, deque.begin());

  it = deque.end();

  it -= 2;
  ASSERT_EQ(*it, 3);
  it -= 2;
  ASSERT_EQ(*it, 1);

  ASSERT_EQ(it, deque.begin());

  it = deque.end();
  it -= deque.size();

  ASSERT_EQ(it, deque.begin());
}

TEST(InlineDeque, OperatorSquareBracket) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (unsigned short i = 0; i < deque.size(); i++) {
    ASSERT_EQ(deque.begin()[i], static_cast<int>(i + 1));
  }
}

TEST(InlineDeque, OperatorLessThan) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < deque.size(); i++) {
    for (int j = 0; j < i; j++) {
      ASSERT_TRUE((deque.begin() + j) < (deque.begin() + i));
    }

    ASSERT_TRUE((deque.begin() + i) < deque.end());
  }
}

TEST(InlineDeque, OperatorLessThanEqual) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < deque.size(); i++) {
    for (int j = 0; j <= i; j++) {
      ASSERT_TRUE((deque.begin() + j) <= (deque.begin() + i));
    }

    ASSERT_TRUE((deque.begin() + i) <= deque.end());
  }
}

TEST(InlineDeque, OperatorGreater) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < deque.size(); i++) {
    for (int j = i + 1; j < deque.size(); j++) {
      ASSERT_TRUE((deque.begin() + j) > (deque.begin() + i));
    }

    ASSERT_TRUE(deque.end() > (deque.begin() + i));
  }
}

TEST(InlineDeque, OperatorGreaterThanEqual) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < deque.size(); i++) {
    for (int j = i; j < deque.size(); j++) {
      ASSERT_TRUE((deque.begin() + j) >= (deque.begin() + i));
    }

    ASSERT_TRUE(deque.end() >= (deque.begin() + i));
  }
}

TEST(InlineDeque, DereferenceOperator) {
  // Content = {0, 0, 1, 2}, Storage = [0, 0, 1, 2]
  InlineDeque<int, 4> deque = {0, 0, 1, 2};
  // Content = {0, 1, 2}, Storage = [x, 0, 1, 2]
  deque.pop_front();
  // Content = {0, 1, 2, 3}, Storage = [3, 0, 1, 2]
  deque.push_back(3);
  // Content = {1, 2, 3}, Storage = [3, x, 1, 2]
  deque.pop_front();
  // Content = {1, 2, 3, 4}, Storage = [3, 4, 1, 2]
  deque.push_back(4);

  for (int i = 0; i < deque.size(); i++) {
    const auto it = deque.begin() + i;
    ASSERT_EQ(*(it.operator->()), static_cast<int>(i + 1));
  }
}

// Test that InlineDeque<T> is trivially destructible when its type is.
static_assert(std::is_trivially_destructible_v<InlineDeque<int, 4>>);

static_assert(std::is_trivially_destructible_v<MoveOnly>);
static_assert(std::is_trivially_destructible_v<InlineDeque<MoveOnly, 1>>);

static_assert(std::is_trivially_destructible_v<CopyOnly>);
static_assert(std::is_trivially_destructible_v<InlineDeque<CopyOnly, 99>>);

static_assert(!std::is_trivially_destructible_v<Counter>);
static_assert(!std::is_trivially_destructible_v<InlineDeque<Counter, 99>>);

// Generic-capacity deques cannot be constructed or destructed.
static_assert(!std::is_constructible_v<InlineDeque<int>>);
static_assert(!std::is_destructible_v<InlineDeque<int>>);

// Tests that InlineDeque<T> does not have any extra padding.
static_assert(sizeof(InlineDeque<uint8_t, 1>) ==
              sizeof(InlineDeque<uint8_t>::size_type) * 4 +
                  std::max(sizeof(InlineDeque<uint8_t>::size_type),
                           sizeof(uint8_t)));
static_assert(sizeof(InlineDeque<uint8_t, 2>) ==
              sizeof(InlineDeque<uint8_t>::size_type) * 4 +
                  2 * sizeof(uint8_t));
static_assert(sizeof(InlineDeque<uint16_t, 1>) ==
              sizeof(InlineDeque<uint16_t>::size_type) * 4 + sizeof(uint16_t));
static_assert(sizeof(InlineDeque<uint32_t, 1>) ==
              sizeof(InlineDeque<uint32_t>::size_type) * 4 + sizeof(uint32_t));
static_assert(sizeof(InlineDeque<uint64_t, 1>) ==
              sizeof(InlineDeque<uint64_t>::size_type) * 4 + sizeof(uint64_t));

// Test that InlineDeque<T> is copy constructible
static_assert(std::is_copy_constructible_v<InlineDeque<int, 4>::iterator>);

// Test that InlineDeque<T> is move constructible
static_assert(std::is_move_constructible_v<InlineDeque<MoveOnly, 4>::iterator>);

// Test that InlineDeque<T> is copy assignable
static_assert(std::is_copy_assignable_v<InlineDeque<int>::iterator>);
static_assert(std::is_copy_assignable_v<InlineDeque<int, 4>::iterator>);

// Test that InlineDeque<T> is move assignable
static_assert(std::is_move_assignable_v<InlineDeque<MoveOnly>>);
static_assert(std::is_move_assignable_v<InlineDeque<MoveOnly, 4>>);

// Test that InlineDeque<T>::iterator can be converted to a const_iterator
static_assert(std::is_convertible<InlineDeque<int>::iterator,
                                  InlineDeque<int>::const_iterator>::value);
static_assert(std::is_convertible<InlineDeque<int, 4>::iterator,
                                  InlineDeque<int, 4>::const_iterator>::value);

// Test that InlineDeque<T>::const_iterator can NOT be converted to a iterator
static_assert(!std::is_convertible<InlineDeque<int>::const_iterator,
                                   InlineDeque<int>::iterator>::value);
static_assert(!std::is_convertible<InlineDeque<int, 4>::const_iterator,
                                   InlineDeque<int, 4>::iterator>::value);

}  // namespace
}  // namespace pw::containers
