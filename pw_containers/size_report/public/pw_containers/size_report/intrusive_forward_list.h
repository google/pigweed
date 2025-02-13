// Copyright 2025 The Pigweed Authors
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
#pragma once

#include "pw_bloat/bloat_this_binary.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_containers/size_report/size_report.h"

namespace pw::containers::size_report {

/// A simple item for intrusive forward lists that wraps a moveable value.
template <typename T>
struct ForwardListItem : public IntrusiveForwardList<ForwardListItem<T>>::Item {
  using Base = typename IntrusiveForwardList<ForwardListItem<T>>::Item;

  constexpr explicit ForwardListItem(T value) : value_(value) {}

  ForwardListItem(ForwardListItem&& other) { *this = std::move(other); }

  ForwardListItem& operator=(ForwardListItem&& other) {
    value_ = other.value_;
    return *this;
  }

  bool operator<(const ForwardListItem& rhs) const {
    return value_ < rhs.value_;
  }

  bool operator==(const ForwardListItem& rhs) const {
    return value_ == rhs.value_;
  }

  T value_;
};

/// Invokes methods of the pw::IntrusiveForwardList type.
///
/// This method is used both to measure intrusive forward lists directly, as
/// well as to provide a baseline for measuring other types that use intrusive
/// forward lists and want to only measure their contributions to code size.
template <typename ItemType>
int MeasureIntrusiveForwardList(uint32_t mask) {
  mask = SetBaseline(mask);
  auto& list1 = GetContainer<IntrusiveForwardList<ItemType>>();
  IntrusiveForwardList<ItemType> list2;
  auto& items = GetItems<ItemType>();
  auto iter1 = items.begin() + 3;
  list1.assign(items.begin(), iter1);
  list2.assign(iter1, items.end());

  auto& item1 = list1.front();
  PW_BLOAT_EXPR(list1.pop_front(), mask);
  PW_BLOAT_EXPR(list2.push_front(item1), mask);
  PW_BLOAT_EXPR(list1.swap(list2), mask);
  PW_BLOAT_EXPR(list1.sort(), mask);
  PW_BLOAT_EXPR(list1.reverse(), mask);
  PW_BLOAT_EXPR(list1.merge(list2), mask);
  PW_BLOAT_EXPR(list2.clear(), mask);
  PW_BLOAT_EXPR(list1.remove(item1), mask);
  PW_BLOAT_COND(list1.unique() != 0, mask);

  ItemType& item2 = list1.front();
  auto iter2 = list1.erase_after(list1.before_begin());
  PW_BLOAT_EXPR(list1.insert_after(iter2, item2), mask);
  PW_BLOAT_EXPR(list1.splice_after(list1.begin(), list2), mask);

  return list1.empty() ? 1 : 0;
}

}  // namespace pw::containers::size_report
