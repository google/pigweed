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

#include <cstdint>

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_containers/intrusive_list.h"
#include "pw_containers/intrusive_map.h"
#include "pw_containers/intrusive_multimap.h"
#include "pw_containers/intrusive_multiset.h"
#include "pw_containers/intrusive_set.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::IntrusiveForwardList;
using pw::IntrusiveMap;
using pw::IntrusiveMultiMap;
using pw::IntrusiveMultiSet;
using pw::IntrusiveSet;
using pw::containers::future::IntrusiveList;

struct ForwardListItem1 : public IntrusiveForwardList<ForwardListItem1>::Item {
};
struct ForwardListItem2 : public IntrusiveForwardList<ForwardListItem2>::Item {
};
struct ListItem1 : public IntrusiveList<ListItem1>::Item {};
struct ListItem2 : public IntrusiveList<ListItem2>::Item {};

struct MapPair1 : public IntrusiveMap<uint32_t, MapPair1>::Pair {
  explicit MapPair1(uint32_t id) : IntrusiveMap<uint32_t, MapPair1>::Pair(id) {}
};
struct MapPair2 : public IntrusiveMap<uint32_t, MapPair2>::Pair {
  explicit MapPair2(uint32_t id) : IntrusiveMap<uint32_t, MapPair2>::Pair(id) {}
};
struct MultiMapPair1 : public IntrusiveMultiMap<uint32_t, MultiMapPair1>::Pair {
  explicit MultiMapPair1(uint32_t id)
      : IntrusiveMultiMap<uint32_t, MultiMapPair1>::Pair(id) {}
};
struct MultiMapPair2 : public IntrusiveMultiMap<uint32_t, MultiMapPair2>::Pair {
  explicit MultiMapPair2(uint32_t id)
      : IntrusiveMultiMap<uint32_t, MultiMapPair2>::Pair(id) {}
};

struct SetItem1 : public IntrusiveSet<SetItem1>::Item {
  bool operator<(const SetItem1& rhs) const { return this < &rhs; }
};
struct SetItem2 : public IntrusiveSet<SetItem2>::Item {
  bool operator<(const SetItem2& rhs) const { return this < &rhs; }
};
struct MultiSetItem1 : public IntrusiveMultiSet<MultiSetItem1>::Item {
  bool operator<(const MultiSetItem1& rhs) const { return this < &rhs; }
};
struct MultiSetItem2 : public IntrusiveMultiSet<MultiSetItem2>::Item {
  bool operator<(const MultiSetItem2& rhs) const { return this < &rhs; }
};

class Base {
 public:
  explicit constexpr Base(const char* name) : name_(name) {}
  constexpr const char* name() const { return name_; }

 private:
  const char* name_;
};

class Derived : public Base,
                public ForwardListItem1,
                public ForwardListItem2,
                public ListItem1,
                public ListItem2,
                public MapPair1,
                public MapPair2,
                public MultiMapPair1,
                public MultiMapPair2,
                public SetItem1,
                public SetItem2,
                public MultiSetItem1,
                public MultiSetItem2 {
 public:
  Derived(const char* name, uint32_t id)
      : Base(name),
        MapPair1(id),
        MapPair2(id),
        MultiMapPair1(id),
        MultiMapPair2(id) {}
};

TEST(IntrusiveItemTest, AddToEachContainerSequentially) {
  Derived item("a", 1);

  IntrusiveForwardList<ForwardListItem1> forward_list1;
  IntrusiveForwardList<ForwardListItem2> forward_list2;
  IntrusiveList<ListItem1> list1;
  IntrusiveList<ListItem2> list2;
  IntrusiveMap<uint32_t, MapPair1> map1;
  IntrusiveMap<uint32_t, MapPair2> map2;
  IntrusiveMultiMap<uint32_t, MultiMapPair1> multimap1;
  IntrusiveMultiMap<uint32_t, MultiMapPair2> multimap2;
  IntrusiveSet<SetItem1> set1;
  IntrusiveSet<SetItem2> set2;
  IntrusiveMultiSet<MultiSetItem1> multiset1;
  IntrusiveMultiSet<MultiSetItem2> multiset2;

  forward_list1.push_front(item);
  forward_list1.clear();

  forward_list2.push_front(item);
  forward_list2.clear();

  list1.push_back(item);
  list1.clear();

  list2.push_back(item);
  list2.clear();

  map1.insert(item);
  map1.clear();

  map2.insert(item);
  map2.clear();

  multimap1.insert(item);
  multimap1.clear();

  multimap2.insert(item);
  multimap2.clear();

  set1.insert(item);
  set1.clear();

  set2.insert(item);
  set2.clear();

  multiset1.insert(item);
  multiset1.clear();

  multiset2.insert(item);
  multiset2.clear();
}

TEST(IntrusiveItemTest, AddToEachContainerSimultaneousy) {
  Derived item("a", 1);

  IntrusiveForwardList<ForwardListItem1> forward_list1;
  IntrusiveForwardList<ForwardListItem2> forward_list2;
  IntrusiveList<ListItem1> list1;
  IntrusiveList<ListItem2> list2;
  IntrusiveMap<uint32_t, MapPair1> map1;
  IntrusiveMap<uint32_t, MapPair2> map2;
  IntrusiveMultiMap<uint32_t, MultiMapPair1> multimap1;
  IntrusiveMultiMap<uint32_t, MultiMapPair2> multimap2;
  IntrusiveSet<SetItem1> set1;
  IntrusiveSet<SetItem2> set2;
  IntrusiveMultiSet<MultiSetItem1> multiset1;
  IntrusiveMultiSet<MultiSetItem2> multiset2;

  forward_list1.push_front(item);
  list1.push_back(item);
  map1.insert(item);
  multimap1.insert(item);
  set1.insert(item);
  multiset1.insert(item);

  forward_list1.clear();
  list1.clear();
  map1.clear();
  multimap1.clear();
  set1.clear();
  multiset1.clear();

  forward_list2.push_front(item);
  list2.push_back(item);
  map2.insert(item);
  multimap2.insert(item);
  set2.insert(item);
  multiset2.insert(item);

  forward_list2.clear();
  list2.clear();
  map2.clear();
  multimap2.clear();
  set2.clear();
  multiset2.clear();
}

#if PW_NC_TEST(ForwardListValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveForwardList<Derived> bad_fwd_list;

#elif PW_NC_TEST(ListValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveList<Derived> bad_list;

#elif PW_NC_TEST(MapValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'key' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("request for member 'key' is ambiguous");
[[maybe_unused]] IntrusiveMap<uint32_t, Derived> bad_map;

#elif PW_NC_TEST(MultiMapValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'key' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("request for member 'key' is ambiguous");
[[maybe_unused]] IntrusiveMultiMap<uint32_t, Derived> bad_multimap;

#elif PW_NC_TEST(SetValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveSet<Derived> bad_set([](const Derived& lhs,
                                                  const Derived& rhs) {
  return &lhs < &rhs;
});

#elif PW_NC_TEST(MultiSetValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveMultiSet<Derived> bad_multiset(
    [](const Derived& lhs, const Derived& rhs) { return &lhs < &rhs; });

#endif  // PW_NC_TEST

}  // namespace
