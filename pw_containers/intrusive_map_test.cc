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

#include "pw_containers/intrusive_map.h"

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_containers/intrusive_multimap.h"
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
struct TestItem : public ::pw::IntrusiveMap<size_t, TestItem>::Item,
                  public BaseItem {
  TestItem(size_t key, const char* name) : BaseItem(key, name) {}
};

// Test fixture.
class IntrusiveMapTest : public ::testing::Test {
 protected:
  using IntrusiveMap = ::pw::IntrusiveMap<size_t, TestItem>;
  static constexpr size_t kNumItems = 10;

  void SetUp() override { map_.insert(items_.begin(), items_.end()); }

  void TearDown() override { map_.clear(); }

  std::array<TestItem, kNumItems> items_ = {{
      {30, "a"},
      {50, "b"},
      {20, "c"},
      {40, "d"},
      {10, "e"},
      {35, "A"},
      {55, "B"},
      {25, "C"},
      {45, "D"},
      {15, "E"},
  }};

  IntrusiveMap map_;
};

// Unit tests.

TEST_F(IntrusiveMapTest, Construct_Default) {
  IntrusiveMap map;
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.begin(), map.end());
  EXPECT_EQ(map.rbegin(), map.rend());
  EXPECT_EQ(map.size(), 0U);
  EXPECT_EQ(map.lower_bound(0), map.end());
  EXPECT_EQ(map.upper_bound(0), map.end());
}

TEST_F(IntrusiveMapTest, Construct_ObjectIterators) {
  map_.clear();
  IntrusiveMap map(items_.begin(), items_.end());
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(map.size(), items_.size());
  map.clear();
}

TEST_F(IntrusiveMapTest, Construct_ObjectIterators_Empty) {
  IntrusiveMap map(items_.end(), items_.end());
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0U);
}

TEST_F(IntrusiveMapTest, Construct_PointerIterators) {
  std::array<TestItem*, 3> ptrs = {&items_[0], &items_[1], &items_[2]};
  map_.clear();
  IntrusiveMap map(ptrs.begin(), ptrs.end());
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(map.size(), 3U);
  map.clear();
}

TEST_F(IntrusiveMapTest, Construct_PointerIterators_Empty) {
  std::array<TestItem*, 0> ptrs;
  IntrusiveMap map(ptrs.begin(), ptrs.end());
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0U);
  map.clear();
}

TEST_F(IntrusiveMapTest, Construct_InitializerList) {
  map_.clear();
  IntrusiveMap map({&items_[0], &items_[2], &items_[4]});
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(map.size(), 3U);
  map.clear();
}

TEST_F(IntrusiveMapTest, Construct_InitializerList_Empty) {
  IntrusiveMap map({});
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0U);
}

TEST_F(IntrusiveMapTest, Construct_CustomCompare) {
  map_.clear();
  using Compare = std::greater<size_t>;
  using CustomMapType = pw::IntrusiveMap<size_t, TestItem, Compare>;

  CustomMapType map(items_.begin(), items_.end());
  EXPECT_EQ(map.size(), items_.size());

  auto iter = map.begin();
  EXPECT_EQ((iter++)->key(), 55U);
  EXPECT_EQ((iter++)->key(), 50U);
  EXPECT_EQ((iter++)->key(), 45U);
  EXPECT_EQ((iter++)->key(), 40U);
  EXPECT_EQ((iter++)->key(), 35U);
  EXPECT_EQ((iter++)->key(), 30U);
  EXPECT_EQ((iter++)->key(), 25U);
  EXPECT_EQ((iter++)->key(), 20U);
  EXPECT_EQ((iter++)->key(), 15U);
  EXPECT_EQ((iter++)->key(), 10U);
  map.clear();
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

TEST_F(IntrusiveMapTest, Construct_CustomGetKey) {
  map_.clear();
  using CustomMapType =
      pw::IntrusiveMap<const char*, TestItem, StrCmp, GetName>;
  CustomMapType map(items_.begin(), items_.end());
  auto iter = map.begin();
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
  EXPECT_EQ(iter, map.end());
  map.clear();
}

//  A struct that is not a map item.
struct NotAnItem : public BaseItem {
  NotAnItem(size_t key, const char* name) : BaseItem(key, name) {}
};

#if PW_NC_TEST(IncompatibleItem)
PW_NC_EXPECT(
    "IntrusiveMap items must be derived from IntrusiveMap<Key, T>::Item");

struct BadItem : public ::pw::IntrusiveMap<size_t, NotAnItem>::Item {};

[[maybe_unused]] ::pw::IntrusiveMap<size_t, BadItem> bad_map1;

#elif PW_NC_TEST(DoesNotInheritFromItem)
PW_NC_EXPECT(
    "IntrusiveMap items must be derived from IntrusiveMap<Key, T>::Item");

[[maybe_unused]] ::pw::IntrusiveMap<size_t, NotAnItem> bad_map2;

#endif  // PW_NC_TEST

// Element access

TEST_F(IntrusiveMapTest, At) {
  const IntrusiveMap& map = map_;
  for (const auto& item : items_) {
    EXPECT_EQ(&(map.at(item.key())), &item);
  }
}

// Iterators

TEST_F(IntrusiveMapTest, Iterator) {
  const IntrusiveMap& map = map_;
  auto iter = map.begin();
  size_t key = 10;
  for (size_t i = 0; i < kNumItems; ++i) {
    auto& item = *iter++;
    EXPECT_EQ(item.key(), key);
    key += 5;
  }
  EXPECT_EQ(key, 60U);
  EXPECT_EQ(iter, map.end());
  EXPECT_EQ(iter, map.cend());
  for (size_t i = 0; i < kNumItems; ++i) {
    key -= 5;
    EXPECT_EQ((--iter)->key(), key);
  }
  EXPECT_EQ(key, 10U);
  EXPECT_EQ(iter, map.begin());
  EXPECT_EQ(iter, map.cbegin());
}

TEST_F(IntrusiveMapTest, ReverseIterator) {
  const IntrusiveMap& map = map_;
  auto iter = map.rbegin();
  size_t key = 55;
  for (size_t i = 0; i < kNumItems; ++i) {
    auto& item = *iter++;
    EXPECT_EQ(item.key(), key);
    key -= 5;
  }
  EXPECT_EQ(key, 5U);
  EXPECT_EQ(iter, map.rend());
  EXPECT_EQ(iter, map.crend());
  for (size_t i = 0; i < kNumItems; ++i) {
    key += 5;
    EXPECT_EQ((--iter)->key(), key);
  }
  EXPECT_EQ(key, 55U);
  EXPECT_EQ(iter, map.rbegin());
  EXPECT_EQ(iter, map.crbegin());
}

TEST_F(IntrusiveMapTest, ConstIterator_CompareNonConst) {
  EXPECT_EQ(map_.end(), map_.cend());
}

// A map item that is distinct from TestItem
struct OtherItem : public ::pw::IntrusiveMap<size_t, OtherItem>::Item,
                   public BaseItem {
  OtherItem(size_t key, const char* name) : BaseItem(key, name) {}
};

TEST_F(IntrusiveMapTest, ConstIterator_CompareNonConst_CompilationFails) {
  ::pw::IntrusiveMap<size_t, OtherItem> map;
#if PW_NC_TEST(CannotCompareIncompatibleIteratorsEqual)
  PW_NC_EXPECT("map_\.end\(\) == map\.end\(\)");
  static_cast<void>(map_.end() == map.end());
#elif PW_NC_TEST(CannotCompareIncompatibleIteratorsInequal)
  PW_NC_EXPECT("map_\.end\(\) != map\.end\(\)");
  static_cast<void>(map_.end() != map.end());
#endif  // PW_NC_TEST
}

#if PW_NC_TEST(CannotModifyThroughConstIterator)
PW_NC_EXPECT("function is not marked const|discards qualifiers");

TEST_F(IntrusiveMapTest, ConstIterator_Modify) {
  const IntrusiveMap& map = map_;
  auto iter = map.begin();
  iter->set_name("nope");
}

#endif  // PW_NC_TEST

// Capacity

TEST_F(IntrusiveMapTest, IsEmpty) {
  const IntrusiveMap& map = map_;
  EXPECT_FALSE(map.empty());
  map_.clear();
  EXPECT_TRUE(map.empty());
}

TEST_F(IntrusiveMapTest, GetSize) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.size(), kNumItems);
  map_.clear();
  EXPECT_EQ(map.size(), 0U);
}

TEST_F(IntrusiveMapTest, GetMaxSize) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.max_size(), size_t(std::numeric_limits<ptrdiff_t>::max()));
}

// Modifiers

// This functions allows tests to use `std::is_sorted` without specializing
// `std::less<TestItem>`. Since `std::less` is the default value for the
// `Compare` template parameter, leaving it untouched avoids accidentally
// masking type-handling errors.
constexpr bool LessThan(const TestItem& lhs, const TestItem& rhs) {
  return lhs.key() < rhs.key();
}

TEST_F(IntrusiveMapTest, Insert) {
  map_.clear();
  bool sorted = true;
  size_t prev_key = 0;
  for (auto& item : items_) {
    sorted &= prev_key < item.key();

    // Use the "hinted" version of insert.
    map_.insert(map_.end(), item);
    prev_key = item.key();
  }
  EXPECT_FALSE(sorted);

  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_Duplicate) {
  TestItem item1(60, "1");
  TestItem item2(60, "2");

  auto result = map_.insert(item1);
  EXPECT_STREQ(result.first->name(), "1");
  EXPECT_TRUE(result.second);

  result = map_.insert(item2);
  EXPECT_STREQ(result.first->name(), "1");
  EXPECT_FALSE(result.second);

  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));

  // Explicitly clear the map before item 1 goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Insert_ObjectIterators) {
  map_.clear();
  map_.insert(items_.begin(), items_.end());
  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_ObjectIterators_Empty) {
  map_.insert(items_.end(), items_.end());
  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_ObjectIterators_WithDuplicates) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};

  map_.insert(items.begin(), items.end());
  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Insert_PointerIterators) {
  map_.clear();
  std::array<TestItem*, 3> ptrs = {&items_[0], &items_[1], &items_[2]};

  map_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(map_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_PointerIterators_Empty) {
  std::array<TestItem*, 0> ptrs;

  map_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_PointerIterators_WithDuplicates) {
  TestItem item1(50, "B");
  TestItem item2(40, "D");
  TestItem item3(60, "F");
  std::array<TestItem*, 3> ptrs = {&item1, &item2, &item3};

  map_.insert(ptrs.begin(), ptrs.end());
  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Insert_InitializerList) {
  map_.clear();
  map_.insert({&items_[0], &items_[2], &items_[4]});
  EXPECT_EQ(map_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_InitializerList_Empty) {
  map_.insert({});
  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
}

TEST_F(IntrusiveMapTest, Insert_InitializerList_WithDuplicates) {
  TestItem item1(50, "B");
  TestItem item2(40, "D");
  TestItem item3(60, "F");

  map_.insert({&item1, &item2, &item3});
  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

// An item derived from TestItem.
struct DerivedItem : public TestItem {
  DerivedItem(size_t n, const char* name) : TestItem(n * 10, name) {}
};

TEST_F(IntrusiveMapTest, Insert_DerivedItems) {
  DerivedItem item1(6, "f");
  map_.insert(item1);

  DerivedItem item2(7, "g");
  map_.insert(item2);

  EXPECT_EQ(map_.size(), kNumItems + 2);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Insert_DerivedItems_CompilationFails) {
  ::pw::IntrusiveMap<size_t, DerivedItem> derived_from_compatible_item_type;

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

TEST_F(IntrusiveMapTest, Erase_OneItem) {
  for (size_t i = 0; i < kNumItems; ++i) {
    EXPECT_EQ(map_.size(), kNumItems);
    EXPECT_EQ(map_.erase(items_[i].key()), 1U);
    EXPECT_EQ(map_.size(), kNumItems - 1);
    auto iter = map_.find(items_[i].key());
    EXPECT_EQ(iter, map_.end());
    map_.insert(items_[i]);
  }
}

TEST_F(IntrusiveMapTest, Erase_OnlyItem) {
  map_.clear();
  map_.insert(items_[0]);
  EXPECT_EQ(map_.size(), 1U);

  EXPECT_EQ(map_.erase(items_[0].key()), 1U);
  EXPECT_EQ(map_.size(), 0U);
}

TEST_F(IntrusiveMapTest, Erase_AllOnebyOne) {
  auto iter = map_.begin();
  for (size_t n = kNumItems; n != 0; --n) {
    ASSERT_NE(iter, map_.end());
    iter = map_.erase(iter);
  }
  EXPECT_EQ(iter, map_.end());
  EXPECT_EQ(map_.size(), 0U);
}

TEST_F(IntrusiveMapTest, Erase_Range) {
  auto first = map_.begin();
  auto last = map_.end();
  ++first;
  --last;
  auto iter = map_.erase(first, last);
  EXPECT_EQ(map_.size(), 2U);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_EQ(iter->key(), 55U);
}

TEST_F(IntrusiveMapTest, Erase_MissingItem) { EXPECT_EQ(map_.erase(100), 0U); }

TEST_F(IntrusiveMapTest, Erase_Reinsert) {
  EXPECT_EQ(map_.size(), items_.size());

  EXPECT_EQ(map_.erase(items_[0].key()), 1U);
  EXPECT_EQ(map_.find(items_[0].key()), map_.end());

  EXPECT_EQ(map_.erase(items_[2].key()), 1U);
  EXPECT_EQ(map_.find(items_[2].key()), map_.end());

  EXPECT_EQ(map_.erase(items_[4].key()), 1U);
  EXPECT_EQ(map_.find(items_[4].key()), map_.end());

  EXPECT_EQ(map_.size(), items_.size() - 3);

  map_.insert(items_[4]);
  auto iter = map_.find(items_[4].key());
  EXPECT_NE(iter, map_.end());

  map_.insert(items_[0]);
  iter = map_.find(items_[0].key());
  EXPECT_NE(iter, map_.end());

  map_.insert(items_[2]);
  iter = map_.find(items_[2].key());
  EXPECT_NE(iter, map_.end());

  EXPECT_EQ(map_.size(), items_.size());
}

TEST_F(IntrusiveMapTest, Swap) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  IntrusiveMap map(items.begin(), items.end());

  map_.swap(map);
  EXPECT_EQ(map.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map.begin(), map.end(), LessThan));
  EXPECT_EQ(map.at(30).name(), "a");
  EXPECT_EQ(map.at(50).name(), "b");
  EXPECT_EQ(map.at(20).name(), "c");
  EXPECT_EQ(map.at(40).name(), "d");
  EXPECT_EQ(map.at(10).name(), "e");
  map.clear();

  EXPECT_EQ(map_.size(), 3U);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(50).name(), "B");
  EXPECT_STREQ(map_.at(40).name(), "D");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Swap_Empty) {
  IntrusiveMap map;

  map_.swap(map);
  EXPECT_EQ(map.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map.begin(), map.end(), LessThan));
  EXPECT_EQ(map.at(30).name(), "a");
  EXPECT_EQ(map.at(50).name(), "b");
  EXPECT_EQ(map.at(20).name(), "c");
  EXPECT_EQ(map.at(40).name(), "d");
  EXPECT_EQ(map.at(10).name(), "e");
  map.clear();

  EXPECT_EQ(map_.size(), 0U);
}

TEST_F(IntrusiveMapTest, Merge) {
  std::array<TestItem, 3> items = {{
      {5, "f"},
      {75, "g"},
      {85, "h"},
  }};
  IntrusiveMap map(items.begin(), items.end());

  map_.merge(map);
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map_.size(), kNumItems + 3);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(30).name(), "a");
  EXPECT_STREQ(map_.at(35).name(), "A");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(55).name(), "B");
  EXPECT_STREQ(map_.at(20).name(), "c");
  EXPECT_STREQ(map_.at(25).name(), "C");
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(45).name(), "D");
  EXPECT_STREQ(map_.at(10).name(), "e");
  EXPECT_STREQ(map_.at(15).name(), "E");
  EXPECT_STREQ(map_.at(5).name(), "f");
  EXPECT_STREQ(map_.at(75).name(), "g");
  EXPECT_STREQ(map_.at(85).name(), "h");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Merge_Empty) {
  IntrusiveMap map;

  map_.merge(map);
  EXPECT_EQ(map_.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));

  map.merge(map_);
  EXPECT_TRUE(map_.empty());
  EXPECT_EQ(map.size(), kNumItems);
  EXPECT_TRUE(std::is_sorted(map.begin(), map.end(), LessThan));

  map.clear();
}

TEST_F(IntrusiveMapTest, Merge_WithDuplicates) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  IntrusiveMap map(items.begin(), items.end());

  map_.merge(map);
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(30).name(), "a");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(20).name(), "c");
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(10).name(), "e");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

// A struct for a multimap instead of a map.
struct MultiMapItem
    : public ::pw::IntrusiveMultiMap<size_t, MultiMapItem>::Item,
      public BaseItem {
  MultiMapItem(size_t key, const char* name) : BaseItem(key, name) {}
};

TEST_F(IntrusiveMapTest, Merge_MultiMap) {
  std::array<TestItem, 3> items = {{
      {50, "B"},
      {40, "D"},
      {60, "F"},
  }};
  ::pw::IntrusiveMultiMap<size_t, MultiMapItem> multimap(items.begin(),
                                                         items.end());

  map_.merge(multimap);
  EXPECT_TRUE(multimap.empty());
  EXPECT_EQ(map_.size(), kNumItems + 1);
  EXPECT_TRUE(std::is_sorted(map_.begin(), map_.end(), LessThan));
  EXPECT_STREQ(map_.at(30).name(), "a");
  EXPECT_STREQ(map_.at(50).name(), "b");
  EXPECT_STREQ(map_.at(20).name(), "c");
  EXPECT_STREQ(map_.at(40).name(), "d");
  EXPECT_STREQ(map_.at(10).name(), "e");
  EXPECT_STREQ(map_.at(60).name(), "F");

  // Explicitly clear the map before items goes out of scope.
  map_.clear();
}

TEST_F(IntrusiveMapTest, Count) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.count(10), 1U);
  EXPECT_EQ(map.count(20), 1U);
  EXPECT_EQ(map.count(30), 1U);
  EXPECT_EQ(map.count(40), 1U);
  EXPECT_EQ(map.count(50), 1U);
}

TEST_F(IntrusiveMapTest, Count_NoSuchKey) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.count(60), 0U);
}

TEST_F(IntrusiveMapTest, Find) {
  const IntrusiveMap& map = map_;
  size_t key = 10;
  for (size_t i = 0; i < kNumItems; ++i) {
    auto iter = map.find(key);
    ASSERT_NE(iter, map.end());
    EXPECT_EQ(iter->key(), key);
    key += 5;
  }
}

TEST_F(IntrusiveMapTest, Find_NoSuchKey) {
  const IntrusiveMap& map = map_;
  auto iter = map.find(60);
  EXPECT_EQ(iter, map.end());
}

TEST_F(IntrusiveMapTest, LowerBound) {
  const IntrusiveMap& map = map_;
  auto iter = map.lower_bound(10);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = map.lower_bound(20);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = map.lower_bound(30);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = map.lower_bound(40);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = map.lower_bound(50);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMapTest, LowerBound_NoExactKey) {
  const IntrusiveMap& map = map_;
  auto iter = map.lower_bound(6);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = map.lower_bound(16);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = map.lower_bound(26);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = map.lower_bound(36);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = map.lower_bound(46);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMapTest, LowerBound_OutOfRange) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.lower_bound(56), map.end());
}

TEST_F(IntrusiveMapTest, UpperBound) {
  const IntrusiveMap& map = map_;
  auto iter = map.upper_bound(15);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = map.upper_bound(25);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = map.upper_bound(35);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = map.upper_bound(45);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "b");

  EXPECT_EQ(map.upper_bound(55), map.end());
}

TEST_F(IntrusiveMapTest, UpperBound_NoExactKey) {
  const IntrusiveMap& map = map_;
  auto iter = map.upper_bound(5);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "e");

  iter = map.upper_bound(15);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "c");

  iter = map.upper_bound(25);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "a");

  iter = map.upper_bound(35);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "d");

  iter = map.upper_bound(45);
  ASSERT_NE(iter, map.end());
  EXPECT_STREQ(iter->name(), "b");
}

TEST_F(IntrusiveMapTest, UpperBound_OutOfRange) {
  const IntrusiveMap& map = map_;
  EXPECT_EQ(map.upper_bound(55), map.end());
}

TEST_F(IntrusiveMapTest, EqualRange) {
  const IntrusiveMap& map = map_;

  auto pair = map.equal_range(10);
  IntrusiveMap::const_iterator lower = pair.first;
  IntrusiveMap::const_iterator upper = pair.second;
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "e");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "E");

  std::tie(lower, upper) = map.equal_range(20);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "c");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "C");

  std::tie(lower, upper) = map.equal_range(30);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "a");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "A");

  std::tie(lower, upper) = map.equal_range(40);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "d");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "D");

  std::tie(lower, upper) = map.equal_range(50);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "b");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "B");
}

TEST_F(IntrusiveMapTest, EqualRange_NoExactKey) {
  const IntrusiveMap& map = map_;

  auto pair = map.equal_range(6);
  IntrusiveMap::const_iterator lower = pair.first;
  IntrusiveMap::const_iterator upper = pair.second;
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "e");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "e");

  std::tie(lower, upper) = map.equal_range(16);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "c");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "c");

  std::tie(lower, upper) = map.equal_range(26);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "a");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "a");

  std::tie(lower, upper) = map.equal_range(36);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "d");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "d");

  std::tie(lower, upper) = map.equal_range(46);
  ASSERT_NE(lower, map.end());
  EXPECT_STREQ(lower->name(), "b");
  ASSERT_NE(upper, map.end());
  EXPECT_STREQ(upper->name(), "b");
}

TEST_F(IntrusiveMapTest, EqualRange_OutOfRange) {
  const IntrusiveMap& map = map_;

  auto pair = map.equal_range(56);
  IntrusiveMap::const_iterator lower = pair.first;
  IntrusiveMap::const_iterator upper = pair.second;
  EXPECT_EQ(lower, map.end());
  EXPECT_EQ(upper, map.end());
}

}  // namespace
