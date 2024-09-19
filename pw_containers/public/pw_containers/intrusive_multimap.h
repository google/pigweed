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
#pragma once

#include "pw_containers/internal/aa_tree.h"
#include "pw_containers/internal/aa_tree_item.h"
#include "pw_containers/internal/intrusive.h"

namespace pw {

// Forward declaration for friending.
template <typename, typename, typename, typename>
class IntrusiveMap;

/// A `std::multimap<Key, T, Compare>`-like class that uses intrusive items.
///
/// Since the map structure is stored in the items themselves, each item must
/// outlive any map it is a part of and must be part of at most one map.
///
/// This map requires unique keys. Attempting to add an item with same key as an
/// item already in the map will fail.
///
/// Do not use this base class directly. Use one of the specialization of
/// `pw::IntrsuiveMap` instead.
///
/// * Since items are not allocated by this class, the following methods have
///   no analogue:
///     std::multimap<T>::operator=
///     std::multimap<T>::get_allocator
///     std::multimap<T>::emplace
///     std::multimap<T>::emplace_hint
///
/// * Methods corresponding to the following take initializer lists of pointer
///   to items rather than the itenms themselves:
///     std::multimap<T>::(constructor)
///     std::multimap<T>::insert
///
/// * There are no overloads corresponding to the following methods that take
///   r-value references.:
///     std::multimap<T>::insert
///     std::multimap<T>::merge
///
/// * Since modifying the map modifies the items themselves, methods
///   corresponding to those below only take `iterator`s and not
///   `const_iterator`s:
///     std::multimap<T>::insert
///     std::multimap<T>::erase
///
/// * C++23 methods are not (yet) supported.
///
/// @tparam   Key         Type to sort items on
/// @tparam   T           Type of values stored in the map.
/// @tparam   GetKey      Function with signature `Key(const T&)` that
///                       returns the value that items are sorted on.
/// @tparam   Compare     Function with the signature `bool(Key, Key) that is
///                       used to order items.
template <typename Key,
          typename T,
          typename Compare = std::less<Key>,
          typename GetKey = containers::internal::GetKey<Key, T>>
class IntrusiveMultiMap {
 private:
  using ItemBase = containers::internal::AATreeItem;
  using GenericIterator = containers::internal::GenericAATree::iterator;
  using Tree = containers::internal::AATree<GetKey, Compare>;

 public:
  class Item : public ItemBase {
   public:
    constexpr explicit Item() = default;

   private:
    // GetElementTypeFromItem is used to find the element type from an item.
    // It is used to ensure list items inherit from the correct Item type.
    template <typename, typename, bool>
    friend struct containers::internal::GetElementTypeFromItem;
    using ElementType = T;
  };

  using key_type = typename Tree::Key;
  using mapped_type = std::remove_cv_t<T>;
  using value_type = Item;
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using key_compare = Compare;

 public:
  class iterator : public containers::internal::AATreeIterator<T> {
   public:
    constexpr iterator() = default;

   private:
    using Base = containers::internal::AATreeIterator<T>;
    friend IntrusiveMultiMap;
    constexpr explicit iterator(GenericIterator iter) : Base(iter) {}
  };

  class const_iterator
      : public containers::internal::AATreeIterator<std::add_const_t<T>> {
   public:
    constexpr const_iterator() = default;

   private:
    using Base = containers::internal::AATreeIterator<std::add_const_t<T>>;
    friend IntrusiveMultiMap;
    constexpr explicit const_iterator(GenericIterator iter) : Base(iter) {}
  };

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /// Constructs an empty multimap.
  constexpr IntrusiveMultiMap() : tree_(false) { CheckItemType(); }

  /// Constructs an IntrusiveMultiMap from an iterator over Items.
  ///
  /// The iterator may dereference as either Item& (e.g. from std::array<Item>)
  /// or Item* (e.g. from std::initializer_list<Item*>).
  template <typename Iterator>
  IntrusiveMultiMap(Iterator first, Iterator last) : IntrusiveMultiMap() {
    tree_.insert(first, last);
  }

  /// Constructs an IntrusiveMultiMap from a std::initializer_list of pointers
  /// to items.
  IntrusiveMultiMap(std::initializer_list<Item*> items)
      : IntrusiveMultiMap(items.begin(), items.end()) {}

  // Iterators

  iterator begin() noexcept { return iterator(tree_.begin()); }
  const_iterator begin() const noexcept {
    return const_iterator(tree_.begin());
  }
  const_iterator cbegin() const noexcept { return begin(); }

  iterator end() noexcept { return iterator(tree_.end()); }
  const_iterator end() const noexcept { return const_iterator(tree_.end()); }
  const_iterator cend() const noexcept { return end(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crend() const noexcept { return rend(); }

  // Capacity

  /// @copydoc GenericAATree::empty
  [[nodiscard]] bool empty() const noexcept { return tree_.empty(); }

  /// @copydoc GenericAATree::size
  size_t size() const { return tree_.size(); }

  /// @copydoc GenericAATree::max_size
  constexpr size_t max_size() const noexcept { return tree_.max_size(); }

  // Modifiers

  /// @copydoc GenericAATree::clear
  void clear() { tree_.clear(); }

  /// @copydoc AATree<GetKey, Compare>::insert
  iterator insert(T& item) { return iterator(tree_.insert(item).first); }
  iterator insert(iterator, T& item) {
    // Disregard the hint.
    return iterator(insert(item));
  }

  template <class Iterator>
  void insert(Iterator first, Iterator last) {
    tree_.insert(first, last);
  }

  void insert(std::initializer_list<T*> ilist) {
    tree_.insert(ilist.begin(), ilist.end());
  }

  /// @copydoc GenericAATree::clear
  iterator erase(iterator pos) { return iterator(tree_.erase(*pos)); }

  iterator erase(iterator first, iterator last) {
    return iterator(tree_.erase(*first, *last));
  }

  size_t erase(const Key& key) { return tree_.erase(key); }

  /// @copydoc GenericAATree::swap
  void swap(IntrusiveMultiMap<Key, T, Compare, GetKey>& other) {
    tree_.swap(other.tree_);
  }

  /// @copydoc AATree<GetKey, Compare>::merge
  template <typename MapType>
  void merge(MapType& other) {
    tree_.merge(other.tree_);
  }

  /// @copydoc AATree<GetKey, Compare>::count
  size_t count(const Key& key) const { return tree_.count(key); }

  /// @copydoc AATree<GetKey, Compare>::find
  iterator find(const Key& key) { return iterator(tree_.find(key)); }

  const_iterator find(const Key& key) const {
    return const_iterator(tree_.find(key));
  }

  /// @copydoc AATree<GetKey, Compare>::equal_range
  std::pair<iterator, iterator> equal_range(const Key& key) {
    auto result = tree_.equal_range(key);
    return std::make_pair(iterator(result.first), iterator(result.second));
  }
  std::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
    auto result = tree_.equal_range(key);
    return std::make_pair(const_iterator(result.first),
                          const_iterator(result.second));
  }

  /// @copydoc AATree<GetKey, Compare>::lower_bound
  iterator lower_bound(const Key& key) {
    return iterator(tree_.lower_bound(key));
  }
  const_iterator lower_bound(const Key& key) const {
    return const_iterator(tree_.lower_bound(key));
  }

  /// @copydoc AATree<GetKey, Compare>::upper_bound
  iterator upper_bound(const Key& key) {
    return iterator(tree_.upper_bound(key));
  }
  const_iterator upper_bound(const Key& key) const {
    return const_iterator(tree_.upper_bound(key));
  }

 private:
  // Check that T is an Item in a function, since the class T will not be fully
  // defined when the IntrusiveList<T> class is instantiated.
  static constexpr void CheckItemType() {
    using Base = ::pw::containers::internal::ElementTypeFromItem<ItemBase, T>;
    static_assert(
        std::is_base_of<Base, T>(),
        "IntrusiveMultiMap items must be derived from "
        "IntrusiveMultiMap<Key, T>::Item, where T is the item or one of its "
        "bases.");
  }

  // Allow maps to access the tree for `merge`.
  template <typename, typename, typename, typename>
  friend class IntrusiveMap;

  // The AA tree that stores the map.
  //
  // This field is mutable so that it doesn't need const overloads.
  mutable Tree tree_;
};

}  // namespace pw
