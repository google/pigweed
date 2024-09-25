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
#include "pw_unit_test/framework.h"

namespace {

using pw::IntrusiveForwardList;
using pw::IntrusiveMap;
using pw::IntrusiveMapItemWithKey;
using pw::IntrusiveMultiMap;
using pw::containers::future::IntrusiveList;

class ForwardListItem1 : public IntrusiveForwardList<ForwardListItem1>::Item {};
class ForwardListItem2 : public IntrusiveForwardList<ForwardListItem2>::Item {};
class ListItem1 : public IntrusiveList<ListItem1>::Item {};
class ListItem2 : public IntrusiveList<ListItem2>::Item {};
using MapItem1 = IntrusiveMapItemWithKey<uint32_t, 1>;
using MapItem2 = IntrusiveMapItemWithKey<uint32_t, 2>;
using MultiMapItem1 = IntrusiveMapItemWithKey<uint32_t, 3>;
using MultiMapItem2 = IntrusiveMapItemWithKey<uint32_t, 4>;

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
                public MapItem1,
                public MapItem2,
                public MultiMapItem1,
                public MultiMapItem2 {
 public:
  constexpr Derived(const char* name, uint32_t id)
      : Base(name),
        MapItem1(id_),
        MapItem2(id_),
        MultiMapItem1(id_),
        MultiMapItem2(id_),
        id_(id) {}

 private:
  uint32_t id_;
};

TEST(IntrusiveItemTest, AddToEachContainerSequentially) {
  Derived item("a", 1);

  IntrusiveForwardList<ForwardListItem1> forward_list1;
  IntrusiveForwardList<ForwardListItem2> forward_list2;
  IntrusiveList<ListItem1> list1;
  IntrusiveList<ListItem2> list2;
  IntrusiveMap<uint32_t, MapItem1> map1;
  IntrusiveMap<uint32_t, MapItem2> map2;
  IntrusiveMultiMap<uint32_t, MultiMapItem1> multimap1;
  IntrusiveMultiMap<uint32_t, MultiMapItem2> multimap2;

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
}

TEST(IntrusiveItemTest, AddToEachContainerSimultaneousy) {
  Derived item("a", 1);

  IntrusiveForwardList<ForwardListItem1> forward_list1;
  IntrusiveForwardList<ForwardListItem2> forward_list2;
  IntrusiveList<ListItem1> list1;
  IntrusiveList<ListItem2> list2;
  IntrusiveMap<uint32_t, MapItem1> map1;
  IntrusiveMap<uint32_t, MapItem2> map2;
  IntrusiveMultiMap<uint32_t, MultiMapItem1> multimap1;
  IntrusiveMultiMap<uint32_t, MultiMapItem2> multimap2;

  forward_list1.push_front(item);
  list1.push_back(item);
  map1.insert(item);
  multimap1.insert(item);

  forward_list1.clear();
  list1.clear();
  map1.clear();
  multimap1.clear();

  forward_list2.push_front(item);
  list2.push_back(item);
  map2.insert(item);
  multimap2.insert(item);

  forward_list2.clear();
  list2.clear();
  map2.clear();
  multimap2.clear();
}

#if PW_NC_TEST(ForwardListValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveForwardList<Derived> bad_list;

#elif PW_NC_TEST(ListValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveList<Derived> bad_list;

#elif PW_NC_TEST(MapValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveMap<uint32_t, Derived> bad_list;

#elif PW_NC_TEST(MultiMapValueTypeHasMultipleBases)
PW_NC_EXPECT_CLANG(
    "member 'ItemType' found in multiple base classes of different types");
PW_NC_EXPECT_GCC("lookup of 'ItemType' in '{anonymous}::Derived' is ambiguous");
[[maybe_unused]] IntrusiveMultiMap<uint32_t, Derived> bad_list;

#endif  // PW_NC_TEST

}  // namespace
