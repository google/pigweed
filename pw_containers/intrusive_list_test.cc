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

#include "pw_containers/intrusive_list.h"

#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"
#include "pw_preprocessor/util.h"

namespace pw {
namespace {

class TestItem : public IntrusiveList<TestItem>::Item {
 public:
  TestItem() : number_(0) {}
  TestItem(int number) : number_(number) {}
  int GetNumber() const { return number_; }
  void SetNumber(int num) { number_ = num; }

 private:
  int number_;
};

TEST(IntrusiveList, PushOne) {
  constexpr int kMagicValue = 31;
  IntrusiveList<TestItem> test_items;
  TestItem item1(kMagicValue);
  test_items.push_back(item1);
  EXPECT_FALSE(test_items.empty());
  EXPECT_EQ(test_items.front().GetNumber(), kMagicValue);
}

TEST(IntrusiveList, PushThree) {
  IntrusiveList<TestItem> test_items;
  TestItem item1(1);
  TestItem item2(2);
  TestItem item3(3);
  test_items.push_back(item1);
  test_items.push_back(item2);
  test_items.push_back(item3);

  int loop_count = 0;
  for (auto& test_item : test_items) {
    loop_count++;
    EXPECT_EQ(loop_count, test_item.GetNumber());
  }
  EXPECT_EQ(loop_count, 3);
}

TEST(IntrusiveList, IsEmpty) {
  IntrusiveList<TestItem> test_items;
  EXPECT_TRUE(test_items.empty());

  TestItem item1(1);
  test_items.push_back(item1);
  EXPECT_FALSE(test_items.empty());
}

TEST(IntrusiveList, InsertAfter) {
  constexpr int kMagicValue = 42;
  TestItem item_array[20];
  IntrusiveList<TestItem> test_list;
  // Fill the list with TestItem objects that have a value of zero.
  for (size_t i = 0; i < PW_ARRAY_SIZE(item_array); ++i) {
    item_array[i].SetNumber(0);
    test_list.push_back(item_array[i]);
  }

  // Create a test item to insert midway through the list.
  TestItem inserted_item(kMagicValue);

  // Move an iterator to the middle of the list, and then insert the item.
  auto it = test_list.begin();
  size_t expected_index = 1;  // Expected index is iterator index + 1.
  for (size_t i = 0; i < PW_ARRAY_SIZE(item_array) / 2; ++i) {
    it++;
    expected_index++;
  }
  it = test_list.insert_after(it, inserted_item);

  // Ensure the returned iterator from insert_after is the newly inserted
  // element.
  EXPECT_EQ(it->GetNumber(), kMagicValue);

  // Ensure the value is in the expected location (index of the iterator + 1).
  size_t i = 0;
  for (TestItem& item : test_list) {
    if (item.GetNumber() == kMagicValue) {
      EXPECT_EQ(i, expected_index);
    } else {
      EXPECT_EQ(item.GetNumber(), 0);
    }
    i++;
  }

  // Ensure the list didn't break and change sizes.
  EXPECT_EQ(i, PW_ARRAY_SIZE(item_array) + 1);
}

TEST(IntrusiveList, PushFront) {
  constexpr int kMagicValue = 42;
  TestItem item_array[20];
  IntrusiveList<TestItem> test_list;
  // Fill the list with TestItem objects that have a value of zero.
  for (size_t i = 0; i < PW_ARRAY_SIZE(item_array); ++i) {
    item_array[i].SetNumber(0);
    test_list.push_back(item_array[i]);
  }

  // Create a test item to push to the front of the list.
  TestItem pushed_item(kMagicValue);
  test_list.push_front(pushed_item);
  EXPECT_EQ(test_list.front().GetNumber(), kMagicValue);
}

TEST(IntrusiveList, Clear_Empty) {
  IntrusiveList<TestItem> test_list;
  EXPECT_TRUE(test_list.empty());
  test_list.clear();
  EXPECT_TRUE(test_list.empty());
}

TEST(IntrusiveList, Clear_OneItem) {
  IntrusiveList<TestItem> test_list;
  TestItem item(42);
  test_list.push_back(item);
  EXPECT_FALSE(test_list.empty());
  test_list.clear();
  EXPECT_TRUE(test_list.empty());
}

TEST(IntrusiveList, Clear_TwoItems) {
  IntrusiveList<TestItem> test_list;
  TestItem item1(42);
  TestItem item2(42);
  test_list.push_back(item1);
  test_list.push_back(item2);
  EXPECT_FALSE(test_list.empty());
  test_list.clear();
  EXPECT_TRUE(test_list.empty());
}

TEST(IntrusiveList, Clear_ReinsertClearedItems) {
  std::array<TestItem, 20> item_array;
  IntrusiveList<TestItem> test_list;
  EXPECT_TRUE(test_list.empty());
  test_list.clear();
  EXPECT_TRUE(test_list.empty());

  // Fill the list with TestItem objects.
  for (size_t i = 0; i < item_array.size(); ++i) {
    item_array[i].SetNumber(0);
    test_list.push_back(item_array[i]);
  }

  // Remove everything.
  test_list.clear();
  EXPECT_TRUE(test_list.empty());

  // Ensure all the removed elements can still be added back to a list.
  for (size_t i = 0; i < item_array.size(); ++i) {
    item_array[i].SetNumber(0);
    test_list.push_back(item_array[i]);
  }
}

TEST(IntrusiveList, PopFront) {
  constexpr int kValue1 = 32;
  constexpr int kValue2 = 4083;

  IntrusiveList<TestItem> test_list;
  EXPECT_TRUE(test_list.empty());

  TestItem item1(kValue1);
  TestItem item2(kValue2);

  test_list.push_front(item2);
  test_list.push_front(item1);
  test_list.pop_front();
  EXPECT_EQ(test_list.front().GetNumber(), kValue2);
  EXPECT_FALSE(test_list.empty());
  test_list.pop_front();
  EXPECT_TRUE(test_list.empty());
}

TEST(IntrusiveList, PopFrontAndReinsert) {
  constexpr int kValue1 = 32;
  constexpr int kValue2 = 4083;

  IntrusiveList<TestItem> test_list;
  EXPECT_TRUE(test_list.empty());

  TestItem item1(kValue1);
  TestItem item2(kValue2);

  test_list.push_front(item2);
  test_list.push_front(item1);
  test_list.pop_front();
  test_list.push_front(item1);
  EXPECT_EQ(test_list.front().GetNumber(), kValue1);
}

TEST(IntrusiveList, ListFront) {
  IntrusiveList<TestItem> test_items;

  EXPECT_EQ(&test_items.front(), nullptr);

  TestItem item1(1);
  TestItem item2(0);
  TestItem item3(0xffff);

  test_items.push_back(item1);
  test_items.push_back(item2);
  test_items.push_back(item3);

  EXPECT_EQ(&item1, &test_items.front());
  EXPECT_EQ(&item1, &(*test_items.begin()));
}

TEST(IntrusiveList, IteratorIncrement) {
  TestItem item_array[20];
  IntrusiveList<TestItem> test_list;
  for (size_t i = 0; i < PW_ARRAY_SIZE(item_array); ++i) {
    item_array[i].SetNumber(i);
    test_list.push_back(item_array[i]);
  }

  auto it = test_list.begin();
  int i = 0;
  while (it != test_list.end()) {
    if (i == 0) {
      // Test pre-incrementing on the first element.
      EXPECT_EQ((++it)->GetNumber(), item_array[++i].GetNumber());
    } else {
      EXPECT_EQ((it++)->GetNumber(), item_array[i++].GetNumber());
    }
  }
}

TEST(IntrusiveList, ConstIteratorRead) {
  // For this test, items are checked to be non-zero.
  TestItem item1(1);
  TestItem item2(99);
  IntrusiveList<TestItem> test_items;

  const IntrusiveList<TestItem>* const_list = &test_items;

  test_items.push_back(item1);
  test_items.push_back(item2);

  auto it = const_list->begin();
  while (it != const_list->end()) {
    EXPECT_NE(it->GetNumber(), 0);
    it++;
  }
}

#if NO_COMPILE_TESTS
// TODO(pwbug/47): These tests should fail to compile, enable when no-compile
// tests are set up in Pigweed.
TEST(IntrusiveList, ConstIteratorModify) {
  TestItem item1(1);
  TestItem item2(99);
  IntrusiveList<TestItem> test_items;

  const IntrusiveList<TestItem>* const_list = &test_items;

  test_items.push_back(item1);
  test_items.push_back(item2);

  auto it = const_list->begin();
  while (it != const_list->end()) {
    it->SetNumber(0);
    it++;
  }
}
#endif  // NO_COMPILE_TESTS

}  // namespace
}  // namespace pw
