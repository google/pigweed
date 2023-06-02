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
#include <iterator>
#include <type_traits>

namespace pw {

template <typename>
class IntrusiveList;

namespace intrusive_list_impl {

template <typename T, typename I>
class Iterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::forward_iterator_tag;

  constexpr explicit Iterator() : item_(nullptr) {}

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

  template <typename U, typename J>
  constexpr bool operator==(const Iterator<U, J>& rhs) const {
    return item_ == rhs.item_;
  }

  template <typename U, typename J>
  constexpr bool operator!=(const Iterator<U, J>& rhs) const {
    return item_ != rhs.item_;
  }

 private:
  template <typename, typename>
  friend class Iterator;

  template <typename>
  friend class ::pw::IntrusiveList;

  // Only allow IntrusiveList to create iterators that point to something.
  constexpr explicit Iterator(I* item) : item_{item} {}

  I* item_;
};

class List {
 public:
  class Item {
   public:
    /// Items are not copyable.
    Item(const Item&) = delete;

    /// Items are not copyable.
    Item& operator=(const Item&) = delete;

   protected:
    /// Default constructor.
    ///
    /// All other constructors must call this to preserve the invariant that an
    /// item is always part of a cycle.
    constexpr Item() : Item(this) {}

    /// Destructor.
    ///
    /// This is O(n), where "n" is the number of items in this object's list.
    ~Item() { unlist(); }

    /// Move constructor.
    ///
    /// This uses the move assignment operator. See the note on that method.
    ///
    /// This should NOT typically be used, except for testing.
    Item(Item&& other);

    /// Move assignment operators.
    ///
    /// This will unlist the current object, and replace other with this object
    /// in the list that contained it. As a result, this O(m + n), where "m" is
    /// the number of items in this object's list, and "n" is the number of
    /// objects in the `other` object's list.
    ///
    /// As such, it should NOT typically be used, except for testing.
    Item& operator=(Item&& other);

    /// Returns whether this object is not part of a list.
    ///
    /// This is O(1) whether the object is in a list or not.
    bool unlisted() const { return this == next_; }

    /// Unlink this from the list it is apart of, if any.
    ///
    /// Specifying prev saves calling previous(), which requires looping around
    /// the cycle. This is O(1) with `previous`, and O(n) without.
    void unlist(Item* prev = nullptr);

   private:
    friend class List;

    template <typename T, typename I>
    friend class Iterator;

    /// Explicit constructor.
    ///
    /// In addition to the default constructor for Item, this is used by the
    /// default constructor for List to create its sentinel value.
    explicit constexpr Item(Item* next) : next_(next) {}

    /// Return the previous item in the list by looping around the cycle.
    ///
    /// This is O(n), where "n" is the number of items in this object's list.
    Item* previous();

    // The next pointer. Unlisted items must be self-cycles (next_ == this).
    Item* next_;
  };

  constexpr List() : head_(end()) {}

  template <typename Iterator>
  List(Iterator first, Iterator last) : List() {
    AssignFromIterator(first, last);
  }

  // Intrusive lists cannot be copied, since each Item can only be in one list.
  List(const List&) = delete;
  List& operator=(const List&) = delete;

  template <typename Iterator>
  void assign(Iterator first, Iterator last) {
    clear();
    AssignFromIterator(first, last);
  }

  bool empty() const noexcept { return begin() == end(); }

  /// Inserts an item into a list.
  ///
  /// The item given by `pos` is updated to point to the item as being next in
  /// the list, while the item itself points to what `pos` previously pointed to
  /// as next.
  ///
  /// This is O(1). The ownership of the item is not changed.
  static void insert_after(Item* pos, Item& item);

  /// Removes an item from a list.
  ///
  /// The item that `pos` points to as being next in the list is unlisted, while
  /// `pos` is updated to point to the item following it as next.
  ///
  /// This is O(1). The item is not destroyed..
  static void erase_after(Item* pos);

  void clear();

  bool remove(const Item& item_to_remove);

  /// Returns a pointer to the sentinel item.
  constexpr Item* before_begin() noexcept { return &head_; }
  constexpr const Item* before_begin() const noexcept { return &head_; }

  /// Returns a pointer to the first item.
  constexpr Item* begin() noexcept { return head_.next_; }
  constexpr const Item* begin() const noexcept { return head_.next_; }

  /// Returns a pointer to the last item.
  Item* before_end() noexcept;

  /// Returns a pointer to the sentinel item.
  constexpr Item* end() noexcept { return &head_; }
  constexpr const Item* end() const noexcept { return &head_; }

  /// Returns the number of items in the list by looping around the cycle.
  ///
  /// This is O(n), where "n" is the number of items in the list.
  size_t size() const;

 private:
  /// Adds items to the list from the provided range.
  ///
  /// This is O(n), where "n" is the number of items in the range.
  template <typename Iterator>
  void AssignFromIterator(Iterator first, Iterator last);

  // Use an Item for the head pointer. This gives simpler logic for inserting
  // elements compared to using an Item*. It also makes it possible to use
  // &head_ for end(), rather than nullptr. This makes end() unique for each
  // List and ensures that items already in a list cannot be added to another.
  Item head_;
};

template <typename Iterator>
void List::AssignFromIterator(Iterator first, Iterator last) {
  Item* current = &head_;

  for (Iterator it = first; it != last; ++it) {
    if constexpr (std::is_pointer<std::remove_reference_t<decltype(*it)>>()) {
      insert_after(current, **it);
      current = *it;
    } else {
      insert_after(current, *it);
      current = &(*it);
    }
  }
}

// Gets the element type from an Item. This is used to check that an
// IntrusiveList element class inherits from Item, either directly or through
// another class.
template <typename T, bool kIsItem = std::is_base_of<List::Item, T>()>
struct GetListElementTypeFromItem {
  using Type = void;
};

template <typename T>
struct GetListElementTypeFromItem<T, true> {
  using Type = typename T::PwIntrusiveListElementType;
};

template <typename T>
using ElementTypeFromItem = typename GetListElementTypeFromItem<T>::Type;

}  // namespace intrusive_list_impl
}  // namespace pw
