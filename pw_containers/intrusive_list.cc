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

#include "pw_assert/assert.h"

namespace pw::intrusive_list_impl {

void List::push_back(Item& item) {
  // The incoming item's next is always nullptr, because item is added at the
  // end of the list.
  PW_CHECK_PTR_EQ(item.next_, nullptr);

  if (head_ == nullptr) {
    head_ = &item;
    return;
  }

  Item* current = head_;

  while (current->next_ != nullptr) {
    current = current->next_;
  }

  current->next_ = &item;
}

List::Item& List::insert_after(Item* pos, Item& item) {
  if (pos == nullptr) {
    push_back(item);
    return item;
  }

  // If `next_` of the incoming element is not null, the item is in use and
  // can't be added to this list.
  PW_CHECK_PTR_EQ(item.next_,
                  nullptr,
                  "Cannot add an item to a pw::IntrusiveList when it "
                  "exists in another list");
  item.next_ = pos->next_;
  pos->next_ = &item;
  return item;
}

void List::push_front(Item& item) {
  // If `next_` of the incoming element is not null, the item is in use and
  // can't be added to this list.
  PW_CHECK_PTR_EQ(item.next_,
                  nullptr,
                  "Cannot add an item to a pw::IntrusiveList when it "
                  "exists in another list");
  item.next_ = head_;
  head_ = &item;
}

void List::pop_front() {
  if (head_ == nullptr) {
    return;
  }
  Item* old_head = head_;
  head_ = head_->next_;
  old_head->next_ = nullptr;
}

void List::clear() {
  if (head_ == nullptr) {
    return;
  }

  while (head_->next_ != nullptr) {
    Item* item_to_remove = head_->next_;
    head_->next_ = item_to_remove->next_;
    item_to_remove->next_ = nullptr;
  }

  head_ = nullptr;
}

}  // namespace pw::intrusive_list_impl
