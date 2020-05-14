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

#include <iterator>

namespace pw::intrusive_list_impl {

template <typename T, typename I>
class Iterator {
 public:
  using iterator_category = std::forward_iterator_tag;

  constexpr explicit Iterator() : item_(nullptr) {}
  constexpr explicit Iterator(I* item) : item_{item} {}

  constexpr Iterator& operator++() {
    item_ = static_cast<I*>(item_->next_);
    return *this;
  }

  constexpr Iterator operator++(int) {
    Iterator previous_value(item_);
    operator++();
    return previous_value;
  }

  constexpr const T& operator*() const { return *static_cast<T*>(item_); }
  constexpr T& operator*() { return *static_cast<T*>(item_); }

  constexpr const T* operator->() const { return static_cast<T*>(item_); }
  constexpr T* operator->() { return static_cast<T*>(item_); }

  constexpr bool operator==(const Iterator& rhs) const {
    return item_ == rhs.item_;
  }
  constexpr bool operator!=(const Iterator& rhs) const {
    return item_ != rhs.item_;
  }

 private:
  I* item_;
};

class List {
 public:
  class Item {
   protected:
    constexpr Item() : next_(nullptr) {}

   private:
    friend class List;

    template <typename T, typename I>
    friend class Iterator;

    Item* next_;
  };

  constexpr List() : head_(nullptr) {}

  void push_back(Item& item);

  Item& insert_after(Item* pos, Item& item);

  void push_front(Item& item);

  void pop_front();

  void clear();

  Item* begin() noexcept { return head_; }
  const Item* begin() const noexcept { return head_; }
  const Item* cbegin() const noexcept { return head_; }

 private:
  Item* head_;
};

}  // namespace pw::intrusive_list_impl
