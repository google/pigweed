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

#include "pw_containers/intrusive_multimap.h"

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_containers/intrusive_map.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace {

// Base item.
class BaseItem {
 public:
  BaseItem(size_t key, const char* name) : key_(key), name_(name) {}

  constexpr const size_t& key() const { return key_; }
  constexpr const char* name() const { return name_; }
  void set_name(const char* name) { name_ = name; }

 private:
  size_t key_;
  const char* name_;
};

// A basic item that can be used in a map.
struct TestItem : public ::pw::IntrusiveMultiMap<size_t, TestItem>::Item,
                  public BaseItem {
  TestItem(size_t key, const char* name) : BaseItem(key, name) {}
};

// Test fixture.
class IntrusiveMultiMapTest : public ::testing::Test {
 protected:
  using IntrusiveMultiMap = ::pw::IntrusiveMultiMap<size_t, TestItem>;
  static constexpr size_t kNumItems = 10;

  void SetUp() override { multimap_.insert(items_.begin(), items_.end()); }

  void TearDown() override { multimap_.clear(); }

  std::array<TestItem, kNumItems> items_ = {{
      {30, "a"},
      {50, "b"},
      {20, "c"},
      {40, "d"},
      {10, "e"},
      {30, "A"},
      {50, "B"},
      {20, "C"},
      {40, "D"},
      {10, "E"},
  }};

  IntrusiveMultiMap multimap_;
};

// Unit tests.

TEST_F(IntrusiveMultiMapTest, Construct_Default) {
  IntrusiveMultiMap multimap;
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap.begin(), multimap.end());
  EXPECT_EQ(multimap.rbegin(), multimap.rend());
  EXPECT_EQ(multimap.size(), 0U);
  EXPECT_EQ(multimap.lower_bound(0), multimap.end());
  EXPECT_EQ(multimap.upper_bound(0), multimap.end());
}

TEST_F(IntrusiveMultiMapTest, Construct_ObjectIterators) {
  multimap_.clear();
  IntrusiveMultiMap multimap(items_.begin(), items_.end());
  EXPECT_FALSE(multimap.empty());
  EXPECT_EQ(multimap.size(), items_.size());
  multimap.clear();
}

TEST_F(IntrusiveMultiMapTest, Construct_ObjectIterators_Empty) {
  IntrusiveMultiMap multimap(items_.end(), items_.end());
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, Construct_PointerIterators) {
  std::array<TestItem*, 3> ptrs = {&items_[0], &items_[1], &items_[2]};
  multimap_.clear();
  IntrusiveMultiMap multimap(ptrs.begin(), ptrs.end());
  EXPECT_FALSE(multimap.empty());
  EXPECT_EQ(multimap.size(), 3U);
  multimap.clear();
}

TEST_F(IntrusiveMultiMapTest, Construct_PointerIterators_Empty) {
  std::array<TestItem*, 0> ptrs;
  IntrusiveMultiMap multimap(ptrs.begin(), ptrs.end());
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap.size(), 0U);
  multimap.clear();
}

TEST_F(IntrusiveMultiMapTest, Construct_InitializerList) {
  multimap_.clear();
  IntrusiveMultiMap multimap({&items_[0], &items_[2], &items_[4]});
  EXPECT_FALSE(multimap.empty());
  EXPECT_EQ(multimap.size(), 3U);
  multimap.clear();
}

TEST_F(IntrusiveMultiMapTest, Construct_InitializerList_Empty) {
  IntrusiveMultiMap multimap({});
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, Construct_CustomCompare) {
  multimap_.clear();
  using Compare = std::greater<size_t>;
  using CustomMapType = pw::IntrusiveMultiMap<size_t, TestItem, Compare>;
  CustomMapType multimap(items_.begin(), items_.end());
  auto iter = multimap.begin();
  EXPECT_EQ((iter++)->key(), 50U);
  EXPECT_EQ((iter++)->key(), 50U);
  EXPECT_EQ((iter++)->key(), 40U);
  EXPECT_EQ((iter++)->key(), 40U);
  EXPECT_EQ((iter++)->key(), 30U);
  EXPECT_EQ((iter++)->key(), 30U);
  EXPECT_EQ((iter++)->key(), 20U);
  EXPECT_EQ((iter++)->key(), 20U);
  EXPECT_EQ((iter++)->key(), 10U);
  EXPECT_EQ((iter++)->key(), 10U);
  EXPECT_EQ(iter, multimap.end());
  multimap.clear();
}

// Functor for comparing names.
struct StrCmp {
  bool operator()(const char* lhs, const char* rhs) {
    return std::strcmp(lhs, rhs) < 0;
  }
};

// This functor is similar to `internal::GetKey<size_t, TestName>`, but uses
// the first character of the name instead.
struct GetName {
  const char* operator()(
      const ::pw::containers::internal::AATreeItem& item) const {
    return static_cast<const TestItem&>(item).name();
  }
};

TEST_F(IntrusiveMultiMapTest, Construct_CustomGetKey) {
  multimap_.clear();
  using CustomMapType =
      pw::IntrusiveMultiMap<const char*, TestItem, StrCmp, GetName>;
  CustomMapType multimap(items_.begin(), items_.end());

  auto iter = multimap.begin();
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_EQ(iter, multimap.end());
  multimap.clear();
}

//  A struct that is not a multimap item.
struct NotAnItem : public BaseItem {
  NotAnItem(size_t key, const char* name) : BaseItem(key, name) {}
};

#if PW_NC_TEST(IncompatibleItem)
PW_NC_EXPECT(
    "IntrusiveMultiMap items must be derived from IntrusiveMultiMap<Key, "
    "T>::Item");

class BadItem : public ::pw::IntrusiveMultiMap<size_t, NotAnItem>::Item {};

[[maybe_unused]] ::pw::IntrusiveMultiMap<size_t, BadItem> bad_multimap1;

#elif PW_NC_TEST(DoesNotInheritFromItem)
PW_NC_EXPECT(
    "IntrusiveMultiMap items must be derived from IntrusiveMultiMap<Key, "
    "T>::Item");

[[maybe_unused]] ::pw::IntrusiveMultiMap<size_t, NotAnItem> bad_multimap2;

#endif  // PW_NC_TEST

// Iterators

TEST_F(IntrusiveMultiMapTest, Iterator) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.begin();
  size_t key = 10;
  for (size_t i = 0; i < kNumItems; i += 2) {
    EXPECT_EQ((*iter++).key(), key);
    EXPECT_EQ((*iter++).key(), key);
    key += 10;
  }
  EXPECT_EQ(key, 60U);
  EXPECT_EQ(iter, multimap.end());
  EXPECT_EQ(iter, multimap.cend());
  for (size_t i = 0; i < kNumItems; i += 2) {
    key -= 10;
    EXPECT_EQ((--iter)->key(), key);
    EXPECT_EQ((--iter)->key(), key);
  }
  EXPECT_EQ(key, 10U);
  EXPECT_EQ(iter, multimap.begin());
  EXPECT_EQ(iter, multimap.cbegin());
}

TEST_F(IntrusiveMultiMapTest, ReverseIterator) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.rbegin();
  size_t key = 50;
  for (size_t i = 0; i < kNumItems; i += 2) {
    EXPECT_EQ((*iter++).key(), key);
    EXPECT_EQ((*iter++).key(), key);
    key -= 10;
  }
  EXPECT_EQ(key, 0U);
  EXPECT_EQ(iter, multimap.rend());
  EXPECT_EQ(iter, multimap.crend());
  for (size_t i = 0; i < kNumItems; i += 2) {
    key += 10;
    EXPECT_EQ((--iter)->key(), key);
    EXPECT_EQ((--iter)->key(), key);
  }
  EXPECT_EQ(key, 50U);
  EXPECT_EQ(iter, multimap.rbegin());
  EXPECT_EQ(iter, multimap.crbegin());
}

TEST_F(IntrusiveMultiMapTest, ConstIterator_CompareNonConst) {
  EXPECT_EQ(multimap_.end(), multimap_.cend());
}

// A multimap item that is distinct from TestItem
struct OtherItem : public ::pw::IntrusiveMultiMap<size_t, OtherItem>::Item,
                   public BaseItem {
  OtherItem(size_t key, const char* name) : BaseItem(key, name) {}
};

TEST_F(IntrusiveMultiMapTest, ConstIterator_CompareNonConst_CompilationFails) {
  ::pw::IntrusiveMultiMap<size_t, OtherItem> multimap;
#if PW_NC_TEST(CannotCompareIncompatibleIteratorsEqual)
  PW_NC_EXPECT("multimap_\.end\(\) == multimap\.end\(\)");
  static_cast<void>(multimap_.end() == multimap.end());
#elif PW_NC_TEST(CannotCompareIncompatibleIteratorsInequal)
  PW_NC_EXPECT("multimap_\.end\(\) != multimap\.end\(\)");
  static_cast<void>(multimap_.end() != multimap.end());
#endif  // PW_NC_TEST
}

#if PW_NC_TEST(CannotModifyThroughConstIterator)
PW_NC_EXPECT("function is not marked const|discards qualifiers");

TEST_F(IntrusiveMultiMapTest, ConstIterator_Modify) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.begin();
  iter->set_name("nope");
}

#endif  // PW_NC_TEST

// Capacity

TEST_F(IntrusiveMultiMapTest, IsEmpty) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_FALSE(multimap.empty());
  multimap_.clear();
  EXPECT_TRUE(multimap.empty());
}

TEST_F(IntrusiveMultiMapTest, GetSize) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.size(), kNumItems);
  multimap_.clear();
  EXPECT_EQ(multimap.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, GetMaxSize) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.max_size(), size_t(std::numeric_limits<ptrdiff_t>::max()));
}

// Modifiers

// This functions allows tests to use `std::is_sorted` without specializing
// `std::less<TestItem>`. Since `std::less` is the default value for the
// `Compare` template parameter, leaving it untouched avoids accidentally
// masking type-handling errors.
constexpr bool LessThan(const TestItem& lhs, const TestItem& rhs) {
  return lhs.key() < rhs.key();
}

TEST_F(IntrusiveMultiMapTest, Insert) {
  multimap_.clear();
  bool sorted = true;
  size_t prev_key = 0;
  for (auto& item : items_) {
    sorted &= prev_key < item.key();

    // Use the "hinted" version of insert.
    multimap_.insert(multimap_.end(), item);
    prev_key = item.key();
  }
  EXPECT_FALSE(sorted);

  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_Duplicate) {
  TestItem item1(60, "1");
  TestItem item2(60, "2");

  auto iter = multimap_.insert(item1);
  EXPECT_STREQ(iter->name(), "1");

  iter = multimap_.insert(item2);
  EXPECT_STREQ(iter->name(), "2");

  EXPECT_EQ(multimap_.size(), kNumItems + 2);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  // Explicitly clear the multimap before item 1 goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Insert_ObjectIterators) {
  multimap_.clear();
  multimap_.insert(items_.begin(), items_.end());
  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_ObjectIterators_Empty) {
  multimap_.insert(items_.end(), items_.end());
  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_ObjectIterators_WithDuplicates) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};

  multimap_.insert(items.begin(), items.end());
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  auto iter = multimap_.find(40);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ(iter->name(), "D");

  iter = multimap_.find(50);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ(iter->name(), "B");

  iter = multimap_.find(60);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "F");

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Insert_PointerIterators) {
  multimap_.clear();
  std::array<TestItem*, 3> ptrs = {&items_[0], &items_[1], &items_[2]};

  multimap_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(multimap_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_PointerIterators_Empty) {
  std::array<TestItem*, 0> ptrs;

  multimap_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_PointerIterators_WithDuplicates) {
  TestItem item1(50, "B");
  TestItem item2(40, "D");
  TestItem item3(60, "F");
  std::array<TestItem*, 3> ptrs = {&item1, &item2, &item3};

  multimap_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  auto iter = multimap_.find(40);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ(iter->name(), "D");

  iter = multimap_.find(50);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ(iter->name(), "B");

  iter = multimap_.find(60);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "F");

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Insert_InitializerList) {
  multimap_.clear();
  multimap_.insert({&items_[0], &items_[2], &items_[4]});
  EXPECT_EQ(multimap_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_InitializerList_Empty) {
  multimap_.insert({});
  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
}

TEST_F(IntrusiveMultiMapTest, Insert_InitializerList_WithDuplicates) {
  TestItem item1(50, "B");
  TestItem item2(40, "D");
  TestItem item3(60, "F");

  multimap_.insert({&item1, &item2, &item3});
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  auto iter = multimap_.find(40);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ(iter->name(), "D");

  iter = multimap_.find(50);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ(iter->name(), "B");

  iter = multimap_.find(60);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "F");

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

// An item derived from TestItem.
struct DerivedItem : public TestItem {
  DerivedItem(size_t n, const char* name) : TestItem(n * 10, name) {}
};

TEST_F(IntrusiveMultiMapTest, Insert_DerivedItems) {
  DerivedItem item1(6, "f");
  multimap_.insert(item1);

  DerivedItem item2(7, "g");
  multimap_.insert(item2);

  EXPECT_EQ(multimap_.size(), kNumItems + 2);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Insert_DerivedItems_CompilationFails) {
  ::pw::IntrusiveMultiMap<size_t, DerivedItem>
      derived_from_compatible_item_type;

  DerivedItem item1(6, "f");
  derived_from_compatible_item_type.insert(item1);

  EXPECT_EQ(derived_from_compatible_item_type.size(), 1U);

#if PW_NC_TEST(CannotAddBaseClassToDerivedClassMap)
  PW_NC_EXPECT("derived_from_compatible_item_type\.insert\(item2\)");

  TestItem item2(70, "g");
  derived_from_compatible_item_type.insert(item2);
#endif
  derived_from_compatible_item_type.clear();
}

TEST_F(IntrusiveMultiMapTest, Erase_Two_ByKey) {
  constexpr size_t kHalf = kNumItems / 2;
  for (size_t i = 0; i < kHalf; ++i) {
    ASSERT_EQ(items_[i].key(), items_[i + kHalf].key());
    EXPECT_EQ(multimap_.size(), kNumItems);
    EXPECT_EQ(multimap_.erase(items_[i].key()), 2U);
    EXPECT_EQ(multimap_.size(), kNumItems - 2);
    auto iter = multimap_.find(items_[i].key());
    EXPECT_EQ(iter, multimap_.end());
    multimap_.insert(items_[i]);
    multimap_.insert(items_[i + kHalf]);
  }
}

TEST_F(IntrusiveMultiMapTest, Erase_OnlyItem) {
  multimap_.clear();
  multimap_.insert(items_[0]);
  EXPECT_EQ(multimap_.size(), 1U);

  EXPECT_EQ(multimap_.erase(items_[0].key()), 1U);
  EXPECT_EQ(multimap_.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, Erase_AllOnebyOne) {
  auto iter = multimap_.begin();
  for (size_t n = kNumItems; n != 0; --n) {
    ASSERT_NE(iter, multimap_.end());
    iter = multimap_.erase(iter);
  }
  EXPECT_EQ(iter, multimap_.end());
  EXPECT_EQ(multimap_.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, Erase_Range) {
  auto first = multimap_.begin();
  auto last = multimap_.end();
  ++first;
  --last;
  auto iter = multimap_.erase(first, last);
  EXPECT_EQ(multimap_.size(), 2U);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
  EXPECT_EQ(iter->key(), 50U);
}

TEST_F(IntrusiveMultiMapTest, Erase_MissingItem) {
  EXPECT_EQ(multimap_.erase(100), 0U);
}

TEST_F(IntrusiveMultiMapTest, Erase_Reinsert) {
  constexpr size_t kHalf = kNumItems / 2;
  EXPECT_EQ(multimap_.size(), items_.size());

  ASSERT_EQ(items_[0].key(), items_[0 + kHalf].key());
  EXPECT_EQ(multimap_.erase(items_[0].key()), 2U);
  EXPECT_EQ(multimap_.find(items_[0].key()), multimap_.end());

  ASSERT_EQ(items_[2].key(), items_[2 + kHalf].key());
  EXPECT_EQ(multimap_.erase(items_[2].key()), 2U);
  EXPECT_EQ(multimap_.find(items_[2].key()), multimap_.end());

  ASSERT_EQ(items_[4].key(), items_[4 + kHalf].key());
  EXPECT_EQ(multimap_.erase(items_[4].key()), 2U);
  EXPECT_EQ(multimap_.find(items_[4].key()), multimap_.end());

  EXPECT_EQ(multimap_.size(), items_.size() - 6);

  multimap_.insert(items_[4]);
  auto iter = multimap_.find(items_[4].key());
  EXPECT_NE(iter, multimap_.end());

  multimap_.insert(items_[0]);
  iter = multimap_.find(items_[0].key());
  EXPECT_NE(iter, multimap_.end());

  multimap_.insert(items_[2]);
  iter = multimap_.find(items_[2].key());
  EXPECT_NE(iter, multimap_.end());

  EXPECT_EQ(multimap_.size(), items_.size() - 3);
}

TEST_F(IntrusiveMultiMapTest, Erase_Duplicate) {
  TestItem item1(32, "1");
  TestItem item2(32, "2");
  TestItem item3(32, "3");
  multimap_.insert(item1);
  multimap_.insert(item2);
  multimap_.insert(item3);

  auto iter = multimap_.find(32);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "1");

  iter = multimap_.erase(iter);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "2");

  iter = multimap_.erase(iter);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_STREQ(iter->name(), "3");

  multimap_.erase(iter);
  EXPECT_EQ(multimap_.find(32), multimap_.end());
}

TEST_F(IntrusiveMultiMapTest, Swap) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  IntrusiveMultiMap multimap(items.begin(), items.end());

  multimap_.swap(multimap);
  EXPECT_EQ(multimap.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap.begin(), multimap.end(), LessThan));
  auto iter = multimap.begin();
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_EQ(iter, multimap.end());
  multimap.clear();

  EXPECT_EQ(multimap_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
  iter = multimap_.begin();
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_STREQ((iter++)->name(), "F");
  EXPECT_EQ(iter, multimap_.end());

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Swap_Empty) {
  IntrusiveMultiMap multimap;

  multimap_.swap(multimap);
  EXPECT_EQ(multimap.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap.begin(), multimap.end(), LessThan));
  auto iter = multimap.begin();
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_EQ(iter, multimap.end());
  multimap.clear();

  EXPECT_EQ(multimap_.size(), 0U);
}

TEST_F(IntrusiveMultiMapTest, Merge) {
  std::array<TestItem, 3> items = {{
      {5, "f"},
      {75, "g"},
      {85, "h"},
  }};
  IntrusiveMultiMap multimap(items.begin(), items.end());

  multimap_.merge(multimap);
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
  auto iter = multimap_.begin();
  EXPECT_STREQ((iter++)->name(), "f");
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_STREQ((iter++)->name(), "g");
  EXPECT_STREQ((iter++)->name(), "h");
  EXPECT_EQ(iter, multimap_.end());

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Merge_Empty) {
  IntrusiveMultiMap multimap;

  multimap_.merge(multimap);
  EXPECT_EQ(multimap_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));

  multimap.merge(multimap_);
  EXPECT_TRUE(multimap_.empty());
  EXPECT_EQ(multimap.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(multimap.begin(), multimap.end(), LessThan));

  multimap.clear();
}

TEST_F(IntrusiveMultiMapTest, Merge_WithDuplicates) {
  std::array<TestItem, 3> items = {{
      {15, "f"},
      {45, "g"},
      {55, "h"},
  }};
  IntrusiveMultiMap multimap(items.begin(), items.end());

  multimap_.merge(multimap);
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
  auto iter = multimap_.begin();
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "f");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "g");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_STREQ((iter++)->name(), "h");
  EXPECT_EQ(iter, multimap_.end());

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Merge_Map) {
  std::array<TestItem, 3> items = {{
      {15, "f"},
      {45, "g"},
      {55, "h"},
  }};
  ::pw::IntrusiveMap<size_t, TestItem> map(items.begin(), items.end());

  multimap_.merge(map);
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(multimap_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(multimap_.begin(), multimap_.end(), LessThan));
  auto iter = multimap_.begin();
  EXPECT_STREQ((iter++)->name(), "e");
  EXPECT_STREQ((iter++)->name(), "E");
  EXPECT_STREQ((iter++)->name(), "f");
  EXPECT_STREQ((iter++)->name(), "c");
  EXPECT_STREQ((iter++)->name(), "C");
  EXPECT_STREQ((iter++)->name(), "a");
  EXPECT_STREQ((iter++)->name(), "A");
  EXPECT_STREQ((iter++)->name(), "d");
  EXPECT_STREQ((iter++)->name(), "D");
  EXPECT_STREQ((iter++)->name(), "g");
  EXPECT_STREQ((iter++)->name(), "b");
  EXPECT_STREQ((iter++)->name(), "B");
  EXPECT_STREQ((iter++)->name(), "h");
  EXPECT_EQ(iter, multimap_.end());

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Count) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  multimap_.insert(items.begin(), items.end());

  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.count(10), 2U);
  EXPECT_EQ(multimap.count(20), 2U);
  EXPECT_EQ(multimap.count(30), 2U);
  EXPECT_EQ(multimap.count(40), 3U);
  EXPECT_EQ(multimap.count(50), 3U);
  EXPECT_EQ(multimap.count(60), 1U);

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, Count_NoSuchKey) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.count(60), 0U);
}

TEST_F(IntrusiveMultiMapTest, Find) {
  const IntrusiveMultiMap& multimap = multimap_;
  size_t key = 10;
  for (size_t i = 0; i < kNumItems; i += 2) {
    auto iter = multimap.find(key);
    ASSERT_NE(iter, multimap.end());
    EXPECT_EQ(iter->key(), key);
    key += 10;
  }
}

TEST_F(IntrusiveMultiMapTest, Find_NoSuchKey) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.find(60);
  EXPECT_EQ(iter, multimap.end());
}

TEST_F(IntrusiveMultiMapTest, Find_WithDuplicates) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  multimap_.insert(items.begin(), items.end());

  auto iter = multimap_.find(40);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_EQ(iter->key(), 40U);
  EXPECT_EQ((iter++)->name(), "d");
  EXPECT_EQ(iter->key(), 40U);
  EXPECT_EQ(iter->name(), "D");

  iter = multimap_.find(50);
  ASSERT_NE(iter, multimap_.end());
  EXPECT_EQ(iter->key(), 50U);
  EXPECT_EQ((iter++)->name(), "b");
  EXPECT_EQ(iter->key(), 50U);
  EXPECT_EQ(iter->name(), "B");

  // Explicitly clear the multimap before items goes out of scope.
  multimap_.clear();
}

TEST_F(IntrusiveMultiMapTest, LowerBound) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.lower_bound(10);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = multimap.lower_bound(20);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = multimap.lower_bound(30);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = multimap.lower_bound(40);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = multimap.lower_bound(50);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMultiMapTest, LowerBound_NoExactKey) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.lower_bound(5);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = multimap.lower_bound(15);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = multimap.lower_bound(25);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = multimap.lower_bound(35);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = multimap.lower_bound(45);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMultiMapTest, LowerBound_OutOfRange) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.lower_bound(55), multimap.end());
}

TEST_F(IntrusiveMultiMapTest, UpperBound) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.upper_bound(15);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = multimap.upper_bound(25);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = multimap.upper_bound(35);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = multimap.upper_bound(45);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "b");

  EXPECT_EQ(multimap.upper_bound(55), multimap.end());
}

TEST_F(IntrusiveMultiMapTest, UpperBound_NoExactKey) {
  const IntrusiveMultiMap& multimap = multimap_;
  auto iter = multimap.upper_bound(5);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = multimap.upper_bound(15);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = multimap.upper_bound(25);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = multimap.upper_bound(35);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = multimap.upper_bound(45);
  ASSERT_NE(iter, multimap.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMultiMapTest, UpperBound_OutOfRange) {
  const IntrusiveMultiMap& multimap = multimap_;
  EXPECT_EQ(multimap.upper_bound(55), multimap.end());
}

TEST_F(IntrusiveMultiMapTest, EqualRange) {
  const IntrusiveMultiMap& multimap = multimap_;

  auto pair = multimap.equal_range(10);
  IntrusiveMultiMap::const_iterator lower = pair.first;
  IntrusiveMultiMap::const_iterator upper = pair.second;
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "e");
  ASSERT_NE(upper, multimap.end());
  EXPECT_STREQ(upper->name(), "c");
  EXPECT_EQ(std::distance(lower, upper), 2);

  std::tie(lower, upper) = multimap.equal_range(20);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "c");
  ASSERT_NE(upper, multimap.end());
  EXPECT_STREQ(upper->name(), "a");
  EXPECT_EQ(std::distance(lower, upper), 2);

  std::tie(lower, upper) = multimap.equal_range(30);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "a");
  ASSERT_NE(upper, multimap.end());
  EXPECT_STREQ(upper->name(), "d");
  EXPECT_EQ(std::distance(lower, upper), 2);

  std::tie(lower, upper) = multimap.equal_range(40);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "d");
  ASSERT_NE(upper, multimap.end());
  EXPECT_STREQ(upper->name(), "b");
  EXPECT_EQ(std::distance(lower, upper), 2);

  std::tie(lower, upper) = multimap.equal_range(50);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "b");
  EXPECT_EQ(upper, multimap.end());
  EXPECT_EQ(std::distance(lower, upper), 2);
}

TEST_F(IntrusiveMultiMapTest, EqualRange_NoExactKey) {
  const IntrusiveMultiMap& multimap = multimap_;

  auto pair = multimap.equal_range(5);
  IntrusiveMultiMap::const_iterator lower = pair.first;
  IntrusiveMultiMap::const_iterator upper = pair.second;
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "e");
  EXPECT_EQ(lower, upper);

  std::tie(lower, upper) = multimap.equal_range(15);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "c");
  EXPECT_EQ(lower, upper);

  std::tie(lower, upper) = multimap.equal_range(25);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "a");
  EXPECT_EQ(lower, upper);

  std::tie(lower, upper) = multimap.equal_range(35);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "d");
  EXPECT_EQ(lower, upper);

  std::tie(lower, upper) = multimap.equal_range(45);
  ASSERT_NE(lower, multimap.end());
  EXPECT_STREQ(lower->name(), "b");
  EXPECT_EQ(lower, upper);
}

TEST_F(IntrusiveMultiMapTest, EqualRange_OutOfRange) {
  const IntrusiveMultiMap& multimap = multimap_;

  auto pair = multimap.equal_range(60);
  IntrusiveMultiMap::const_iterator lower = pair.first;
  IntrusiveMultiMap::const_iterator upper = pair.second;
  EXPECT_EQ(lower, multimap.end());
  EXPECT_EQ(upper, multimap.end());
}

}  // namespace
