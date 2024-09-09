// Copyright 2021 The Pigweed Authors
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
#include <initializer_list>
#include <iterator>
#include <limits>
#include <type_traits>

#include "pw_containers/config.h"
#include "pw_containers/internal/intrusive_list.h"
#include "pw_containers/internal/intrusive_list_item.h"
#include "pw_containers/internal/intrusive_list_iterator.h"

namespace pw {
namespace containers::internal {

// Forward declaration for friending.
//
// The LegacyIntrusiveList has methods that run in O(n) time and have no
// analogue in `std::forward_list`. If any of these behaviors are needed,
// callers should consider whether a doubly-linked list is more appropriate,
// or whether they should provide the functionality themselves, e.g. by
// tracking size explicitly.
template <typename>
class LegacyIntrusiveList;

}  // namespace containers::internal

/// A singly-list intrusive list.
///
/// IntrusiveForwardList<T> is a handle to access and manipulate the list, and
/// IntrusiveForwardList<T>::Item is the type from which the base class items
/// must derive.
///
/// As a singly-linked list, the overhead required is only `sizeof(T*)`.
/// However, operations such as removal may require O(n) time to walk the length
/// of the list.
///
/// This class is modeled on `std::forward_list`, with the following
/// differences:
///
/// * Since items are not allocated by this class, the following methods have
///   no analogue:
///     std::forward_list<T>::get_allocator
///     std::forward_list<T>::emplace_after
///     std::forward_list<T>::emplace_front
///     std::forward_list<T>::resize
///
/// * Methods corresponding to the following take initializer lists of pointer
///   to items rather than the itenms themselves:
///     std::forward_list<T>::(constructor)
///     std::forward_list<T>::assign
///     std::forward_list<T>::insert_after
///
/// * There are no overloads corresponding to the following methods that take
///   r-value references.:
///     std::forward_list<T>::insert_after
///     std::forward_list<T>::push_front
///     std::forward_list<T>::splice_after
///
/// * Since modifying the list modifies the items themselves, methods
///   corresponding to those below only take `iterator`s and not
///   `const_iterator`s:
///     std::forward_list<T>::insert_after
///     std::forward_list<T>::erase_after
///     std::forward_list<T>::splice_after
///
/// * C++23 methods are not (yet) supported.
///
/// Example:
/// @code{.cpp}
///   class TestItem
///      : public IntrusiveForwardList<TestItem>::Item {}
///
///   IntrusiveForwardList<TestItem> test_items;
///
///   auto item = TestItem();
///   test_items.push_back(item);
///
///   for (auto& test_item : test_items) {
///     // Do a thing.
///   }
/// @endcode
template <typename T>
class IntrusiveForwardList {
 private:
  using ItemBase = ::pw::containers::internal::IntrusiveForwardListItem;

 public:
  using element_type = T;
  using value_type = std::remove_cv_t<element_type>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = element_type*;
  using const_pointer = const element_type*;

  class Item : public ItemBase {
   protected:
    /// The default constructor is the only constructor provided to derived item
    /// types. A derived type provides a move constructor and move assignment
    /// operator to facilitate using the type with e.g. `Vector::emplace_back`,
    /// but this will not modify the list that the item belongs to, if any.
    constexpr explicit Item() = default;

   private:
    // GetListElementTypeFromItem is used to find the element type from an item.
    // It is used to ensure list items inherit from the correct Item type.
    template <typename, typename, bool>
    friend struct ::pw::containers::internal::GetListElementTypeFromItem;

    using PwIntrusiveListElementType = T;
  };

  using iterator =
      typename ::pw::containers::internal::ForwardIterator<T, Item>;
  using const_iterator =
      typename ::pw::containers::internal::ForwardIterator<std::add_const_t<T>,
                                                           const Item>;

  constexpr IntrusiveForwardList() { CheckItemType(); }

  /// Constructs a list from an iterator over items. The iterator may
  /// dereference as either Item& (e.g. from std::array<Item>) or Item* (e.g.
  /// from std::initializer_list<Item*>).
  template <typename Iterator>
  IntrusiveForwardList(Iterator first, Iterator last) {
    list_.assign(first, last);
  }

  /// Constructs a list from a std::initializer_list of pointers to items.
  IntrusiveForwardList(std::initializer_list<Item*> items)
      : IntrusiveForwardList(items.begin(), items.end()) {}

  template <typename Iterator>
  void assign(Iterator first, Iterator last) {
    list_.assign(first, last);
  }

  void assign(std::initializer_list<T*> items) {
    list_.assign(items.begin(), items.end());
  }

  // Element access

  /// Reference to the first element in the list. Undefined behavior if empty().
  reference front() { return *static_cast<T*>(list_.begin()); }
  const_reference front() const {
    return *static_cast<const T*>(list_.begin());
  }

  // Iterators

  iterator before_begin() noexcept {
    return MakeIterator(list_.before_begin());
  }
  const_iterator before_begin() const noexcept {
    return MakeIterator(list_.before_begin());
  }
  const_iterator cbefore_begin() const noexcept { return before_begin(); }

  iterator begin() noexcept { return MakeIterator(list_.begin()); }
  const_iterator begin() const noexcept { return MakeIterator(list_.begin()); }
  const_iterator cbegin() const noexcept { return begin(); }

  iterator end() noexcept { return MakeIterator(list_.end()); }
  const_iterator end() const noexcept { return MakeIterator(list_.end()); }
  const_iterator cend() const noexcept { return end(); }

  // Capacity

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::empty
  [[nodiscard]] bool empty() const noexcept { return list_.empty(); }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::max_size
  constexpr size_type max_size() const noexcept {
    return std::numeric_limits<difference_type>::max();
  }

  // Modifiers

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::clear
  void clear() { list_.clear(); }

  /// Inserts the given `item` after the given position, `pos`.
  template <typename Iterator>
  Iterator insert_after(Iterator pos, T& item) {
    return MakeIterator(list_.insert_after(pos.item_, item));
  }

  /// Inserts the range of items from `first` (inclusive) to `last` (exclusive)
  /// after the given position, `pos`.
  template <typename Iterator>
  iterator insert_after(iterator pos, Iterator first, Iterator last) {
    return MakeIterator(list_.insert_after(pos.item_, first, last));
  }

  /// Inserts the range of items from `first` (inclusive) to `last` (exclusive)
  /// after the given position, `pos`.
  iterator insert_after(iterator pos, std::initializer_list<T*> items) {
    return insert_after(pos, items.begin(), items.end());
  }

  /// Removes the item following pos from the list. The item is not destructed.
  iterator erase_after(iterator pos) {
    return MakeIterator(list_.erase_after(pos.item_));
  }

  /// Removes the range of items from `first` (inclusive) to `last` (exclusive).
  iterator erase_after(iterator first, iterator last) {
    return MakeIterator(list_.erase_after(first.item_, last.item_));
  }

  /// Inserts the item at the start of the list.
  void push_front(T& item) { list_.insert_after(list_.before_begin(), item); }

  /// Removes the first item in the list. The list must not be empty.
  void pop_front() { remove(front()); }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::swap
  void swap(IntrusiveForwardList<T>& other) noexcept {
    list_.swap(other.list_);
  }

  // Operations

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::merge
  ///
  /// This overload uses `T::operator<`.
  void merge(IntrusiveForwardList<T>& other) {
    list_.merge(other.list_, [](const ItemBase& a, const ItemBase& b) -> bool {
      return static_cast<const T&>(a) < static_cast<const T&>(b);
    });
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::merge
  template <typename Compare>
  void merge(IntrusiveForwardList<T>& other, Compare comp) {
    list_.merge(other.list_, [comp](const ItemBase& a, const ItemBase& b) {
      return comp(static_cast<const T&>(a), static_cast<const T&>(b));
    });
  }

  /// Inserts the items of `other` to after `pos` in this list. Upon returning,
  /// `other` will be empty.
  void splice_after(iterator pos, IntrusiveForwardList<T>& other) {
    splice_after(pos, other, other.before_begin(), other.end());
  }

  /// Moves the item pointed to by the iterator *following* `it` from `other`
  /// to after `pos` in this list.
  void splice_after(iterator pos, IntrusiveForwardList<T>& other, iterator it) {
    splice_after(pos, other, it, std::next(std::next(it)));
  }

  /// Moves the items exclusively between `first` and `last` from `other` to
  /// after `pos` in this list.
  void splice_after(iterator pos,
                    IntrusiveForwardList<T>& other,
                    iterator first,
                    iterator last) {
    list_.splice_after(pos.item_, other.list_, first.item_, last.item_);
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::remove
  bool remove(const T& item) { return list_.remove(item); }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::remove_if
  template <typename UnaryPredicate>
  size_type remove_if(UnaryPredicate pred) {
    return list_.remove_if([pred](const ItemBase& item) -> bool {
      return pred(static_cast<const T&>(item));
    });
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::reverse
  void reverse() { list_.reverse(); }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::unique
  ///
  /// This overload uses `T::operator==`.
  size_type unique() {
    return list_.unique([](const ItemBase& a, const ItemBase& b) -> bool {
      return static_cast<const T&>(a) == static_cast<const T&>(b);
    });
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::unique
  template <typename BinaryPredicate>
  size_type unique(BinaryPredicate pred) {
    return list_.unique([pred](const ItemBase& a, const ItemBase& b) {
      return pred(static_cast<const T&>(a), static_cast<const T&>(b));
    });
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::sort
  ///
  /// This overload uses `T::operator<`.
  void sort() {
    list_.sort([](const ItemBase& a, const ItemBase& b) -> bool {
      return static_cast<const T&>(a) < static_cast<const T&>(b);
    });
  }

  /// @copydoc internal::GenericIntrusiveList<ItemBase>::sort
  template <typename Compare>
  void sort(Compare comp) {
    list_.sort([comp](const ItemBase& a, const ItemBase& b) {
      return comp(static_cast<const T&>(a), static_cast<const T&>(b));
    });
  }

 private:
  // Check that T is an Item in a function, since the class T will not be fully
  // defined when the IntrusiveList<T> class is instantiated.
  static constexpr void CheckItemType() {
    using Base = ::pw::containers::internal::ElementTypeFromItem<ItemBase, T>;
    static_assert(
        std::is_base_of<Base, T>(),
        "IntrusiveForwardList items must be derived from "
        "IntrusiveForwardList<T>::Item, where T is the item or one of its "
        "bases.");
  }

  iterator MakeIterator(ItemBase* item) {
    return iterator(static_cast<Item*>(item));
  }

  const_iterator MakeIterator(const ItemBase* item) const {
    return const_iterator(static_cast<const Item*>(item));
  }

  template <typename>
  friend class containers::internal::LegacyIntrusiveList;

  ::pw::containers::internal::GenericIntrusiveList<ItemBase> list_;
};

}  // namespace pw
