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
#pragma once

#include <cstddef>
#include <type_traits>

#include "pw_containers/internal/intrusive_list_impl.h"

namespace pw {

// IntrusiveList provides singly-linked list functionality for derived
// class items. IntrusiveList<T> is a handle to access and manipulate the
// list, and IntrusiveList<T>::Item is a base class items must inherit
// from. An instantiation of the derived class becomes a list item when inserted
// into a IntrusiveList.
//
// This has two important ramifications:
//
// - An instantiated IntrusiveList::Item must remain in scope for the
//   lifetime of the IntrusiveList it has been added to.
// - A linked list item CANNOT be included in two lists, as it is part of a
//   preexisting list and adding it to another implicitly breaks correctness of
//   the first list.
//
// Usage:
//
//   class TestItem
//      : public containers::IntrusiveList<TestItem>::Item {}
//
//   containers::IntrusiveList<TestItem> test_items;
//
//   auto item = TestItem();
//   test_items.push_back(item);
//
//   for (auto& test_item : test_items) {
//     // Do a thing.
//   }
template <typename T>
class IntrusiveList {
 public:
  class Item : public intrusive_list_impl::List::Item {
   public:
    constexpr Item() = default;
  };

  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator = intrusive_list_impl::Iterator<T, Item>;
  using const_iterator =
      intrusive_list_impl::Iterator<std::add_const_t<T>, const Item>;

  [[nodiscard]] bool empty() const noexcept { return list_.begin() == nullptr; }

  void push_back(T& item) { list_.push_back(item); }

  void push_front(T& item) { list_.push_front(item); }

  iterator insert_after(iterator pos, T& item) {
    return iterator(static_cast<Item*>(&list_.insert_after(&(*pos), item)));
  }

  void pop_front() { list_.pop_front(); }

  void clear() { list_.clear(); }

  T& front() { return *static_cast<T*>(list_.begin()); }

  iterator begin() noexcept {
    return iterator(static_cast<Item*>(list_.begin()));
  }
  const_iterator begin() const noexcept {
    return const_iterator(static_cast<const Item*>(list_.cbegin()));
  }
  const_iterator cbegin() const noexcept {
    return const_iterator(static_cast<const Item*>(list_.cbegin()));
  }

  iterator end() noexcept { return iterator(); }
  const_iterator end() const noexcept { return const_iterator(); }
  const_iterator cend() const noexcept { return const_iterator(); }

 private:
  intrusive_list_impl::List list_;
};

}  // namespace pw
