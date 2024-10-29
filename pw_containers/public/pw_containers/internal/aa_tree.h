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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_containers/internal/aa_tree_item.h"
#include "pw_containers/internal/aa_tree_iterator.h"
#include "pw_containers/internal/intrusive_item.h"
#include "pw_function/function.h"

namespace pw::containers::internal {

/// Base type for an AA tree that is devoid of template parameters.
///
/// This generic class does not implement any functionality that requires
/// comparing keys, and should not be used directly. Instead, see the `AATree`
/// class that is templated on methods to get and compare keys.
class GenericAATree {
 public:
  using iterator = AATreeIterator<>;

  /// Constructs an empty AA tree.
  ///
  /// @param  unique_keys   Indicates if this tree requires unique keys or
  ///                       allows duplicate keys.
  constexpr explicit GenericAATree(bool unique_keys)
      : unique_keys_(unique_keys) {}

  /// Destructor.
  ~GenericAATree() { CheckIntrusiveContainerIsEmpty(empty()); }

  // Intrusive trees cannot be copied, since each Item can only be in one tree.
  GenericAATree(const GenericAATree&) = delete;
  GenericAATree& operator=(const GenericAATree&) = delete;

  [[nodiscard]] constexpr bool unique_keys() const { return unique_keys_; }

  /// Sets the tree's root item.
  void SetRoot(AATreeItem* item);

  // Iterators

  /// Returns a pointer to the first item, if any.
  constexpr iterator begin() noexcept {
    return empty() ? iterator(&root_) : iterator(&root_, root_->GetLeftmost());
  }

  /// Returns a pointer to the last item, if any.
  constexpr iterator end() noexcept {
    return empty() ? iterator(&root_)
                   : ++(iterator(&root_, root_->GetRightmost()));
  }

  // Capacity

  /// Returns whether the tree has zero items or not.
  [[nodiscard]] constexpr bool empty() const { return root_ == nullptr; }

  /// Returns the number of items in the tree.
  size_t size() const;

  /// Returns how many items can be added.
  ///
  /// As an intrusive container, this is effectively unbounded.
  constexpr size_t max_size() const noexcept {
    return std::numeric_limits<ptrdiff_t>::max();
  }

  // Modifiers

  /// Removes all items from the tree and leaves it empty.
  ///
  /// The items themselves are not destructed.
  void clear();

  /// Removes an item from the tree and returns an iterator to the item after
  /// the removed item.
  ///
  /// The items themselves are not destructed.
  iterator erase_one(AATreeItem& item);

  /// Removes the items from first, inclusive, to last, exclusive from the tree
  /// and returns an iterator to the item after the last removed item.
  ///
  /// The items themselves are not destructed.
  iterator erase_range(AATreeItem& first, AATreeItem& last);

  /// Exchanges this tree's items with the `other` tree's items.
  void swap(GenericAATree& other);

 private:
  // The derived, templated type needs to be able to access the root element.
  template <typename, typename>
  friend class AATree;

  /// Root of the tree. Only null if the tree is empty.
  AATreeItem* root_ = nullptr;

  /// Indicates whether the tree requires unique keys.
  ///
  /// This is a const member rather than a template parameter for 3 reasons:
  /// 1. It is a tree field and not an item field, meaning the space overhead is
  ///    marginal.
  /// 2. It is only used in a single branch, i.e. when inserting an item with a
  ///    duplicate key. Making this branch constexpr-if would only save a
  ///    minimal amount of code size.
  /// 3. It allows the same types of ``AATree<K,C>``, ``AATree<K,C>::Item`` and
  ///    ``iterator<TT,T>`` to be used for both ``IntrusiveMap`` and
  ///    ``IntrusiveMultiMap``, thereby reducing code size more significantly.
  const bool unique_keys_;
};

/// An AA tree, as described by Arne Andersson in
/// https://user.it.uu.se/~arneande/ps/simp.pdf. AA trees are simplified
/// red-black which offer almost as much performance with much simpler and
/// smaller code.
///
/// The tree provides an ordered collection of keyed items, and is used to
/// implement ``pw::IntrusiveMap`` and ``pw::IntrusiveMultiMap``. Keys are
/// retrieved and compared using the functions provided via template parameters.
///
/// @tparam   Compare     Function with the signature `bool(Key, Key) that is
///                       used to order items.
template <typename K, typename V>
class AATree final : public GenericAATree {
 public:
  using Key = K;
  using Compare = Function<bool(Key, Key)>;
  using GetKey = Function<Key(const V&)>;
  using GenericAATree::iterator;

  /// Base item type for intrusive items stored in trees.
  ///
  /// Unlike `IntrusiveForwardList` and `IntrusiveList`, which define distinct
  /// nested types for their items, maps and sets share the same base item type.
  class Item : public AATreeItem {
   public:
    constexpr explicit Item() = default;

   private:
    // IntrusiveItem is used to ensure T inherits from this container's Item
    // type. See also `CheckItemType`.
    template <typename, typename, bool>
    friend struct IntrusiveItem;
    using ItemType = V;
  };

  /// An extension of `Item` that includes storage for a key.
  class Pair : public Item {
   public:
    constexpr explicit Pair(Key key) : key_(key) {}
    constexpr Key key() const { return key_; }

    constexpr bool operator<(const Pair& rhs) { return key_ < rhs.key(); }

   private:
    const Key key_;
  };

  /// Constructs an empty AA tree.
  ///
  /// @param  unique_keys   Indicates if this tree requires unique keys or
  ///                       allows duplicate keys.
  constexpr AATree(bool unique_keys, Compare&& compare, GetKey&& get_key)
      : GenericAATree(unique_keys),
        compare_(std::move(compare)),
        get_key_(std::move(get_key)) {}

  // Modifiers

  /// Attempts to add the given item to the tree.
  ///
  /// The item will be added if the tree does not already contain an item with
  /// the given item's key or if the tree does not require unique keys.
  ///
  /// @pre      The item must not be a part of any tree.
  ///
  /// @returns  A pointer to the inserted item and `true`, or a pointer to the
  ///           existing item with same key and `false`.
  std::pair<iterator, bool> insert(AATreeItem& item);

  /// Inserts each item from `first`, inclusive, to `last`, exclusive. If the
  /// tree does not all duplicates and an equivalent item is already in the
  /// tree, the item is ignored.
  template <class Iterator>
  void insert(Iterator first, Iterator last);

  using GenericAATree::erase_one;
  using GenericAATree::erase_range;

  /// Removes items that match the given `key`, and returns an iterator to
  /// the item after the removed items and the number of items removed.
  size_t erase_all(Key key);

  /// Splices items from the `other` tree into this one.
  ///
  /// The receiving tree's `Compare` and `GetKey` functions are used when
  /// inserting items.
  void merge(AATree<K, V>& other);

  // Lookup

  /// Returns the number of items in the tree with the given key.
  ///
  /// If the tree requires unique keys, this simply 0 or 1.
  size_t count(Key key);

  /// Returns a pointer to an item with the given key, or null if the tree does
  /// not contain such an item.
  iterator find(Key key);

  /// Returns a pair of items where the first points to the item with the
  /// smallest key that is not less than the given key, and the second points to
  /// the item with the smallest key that is greater than the given key.
  std::pair<iterator, iterator> equal_range(Key key);

  /// Returns the item in the tree with the smallest key that is greater than or
  /// equal to the given key, or null if the tree is empty.
  iterator lower_bound(Key key);

  /// Returns the item in the tree with the smallest key that is strictly
  /// greater than the given key, or null if the tree is empty.
  iterator upper_bound(Key key);

 private:
  /// Inserts the given `child` in the subtree rooted by `parent` and returns
  /// the resulting tree. If the tree does not allow duplicates and an
  /// equivalent item is already in the tree, the tree is unchanged and the
  /// existing item is returned via `duplicate`.
  AATreeItem* InsertImpl(AATreeItem& parent,
                         AATreeItem& child,
                         AATreeItem*& duplicate);

  /// Returns the item in the subtree rooted by the given `item` with the
  /// smallest key that is greater than or equal to the given `key`, or null if
  /// the subtree is empty.
  AATreeItem* GetLowerBoundImpl(AATreeItem* item, Key key);

  /// Returns the item in the subtree rooted by the given `item` with the
  /// smallest key that is strictly greater than the given `key`, or null if the
  /// subtree is empty.
  AATreeItem* GetUpperBoundImpl(AATreeItem* item, Key key);

  /// Returns the key associated with a tree item.
  constexpr Key DoGetKey(const AATreeItem& item) {
    return get_key_(static_cast<const V&>(item));
  }

  Compare compare_;
  GetKey get_key_;
};

// Template method implementations.

template <typename K, typename V>
std::pair<GenericAATree::iterator, bool> AATree<K, V>::insert(
    AATreeItem& item) {
  CheckIntrusiveItemIsUncontained(!item.IsMapped());
  item.SetLevel(1);
  if (empty()) {
    SetRoot(&item);
    return std::make_pair(iterator(&root_, &item), true);
  }
  AATreeItem* duplicate = nullptr;
  SetRoot(InsertImpl(*root_, item, duplicate));
  if (duplicate == nullptr) {
    return std::make_pair(iterator(&root_, &item), true);
  }
  item.Reset();
  return std::make_pair(iterator(&root_, duplicate), false);
}

template <typename K, typename V>
AATreeItem* AATree<K, V>::InsertImpl(AATreeItem& parent,
                                     AATreeItem& child,
                                     AATreeItem*& duplicate) {
  if (compare_(DoGetKey(child), DoGetKey(parent))) {
    AATreeItem* left = parent.left_.get();
    left = left == nullptr ? &child : InsertImpl(*left, child, duplicate);
    parent.SetLeft(left);

  } else if (compare_(DoGetKey(parent), DoGetKey(child)) || !unique_keys()) {
    AATreeItem* right = parent.right_.get();
    right = right == nullptr ? &child : InsertImpl(*right, child, duplicate);
    parent.SetRight(right);

  } else {
    duplicate = &parent;
    return &parent;
  }

  return parent.Skew()->Split();
}

template <typename K, typename V>
template <class Iterator>
void AATree<K, V>::insert(Iterator first, Iterator last) {
  for (Iterator iter = first; iter != last; ++iter) {
    if constexpr (std::is_pointer_v<
                      typename std::iterator_traits<Iterator>::value_type>) {
      insert(**iter);
    } else {
      insert(*iter);
    }
  }
}

template <typename K, typename V>
size_t AATree<K, V>::erase_all(Key key) {
  size_t removed = 0;
  iterator iter = lower_bound(key);
  while (iter != end() && !compare_(key, DoGetKey(*iter))) {
    iter = erase_one(*iter);
    ++removed;
  }
  return removed;
}

template <typename K, typename V>
void AATree<K, V>::merge(AATree<K, V>& other) {
  auto iter = other.begin();
  while (!other.empty()) {
    AATreeItem& item = *iter;
    iter = other.erase_one(item);
    insert(item);
  }
}

template <typename K, typename V>
size_t AATree<K, V>::count(Key key) {
  return std::distance(lower_bound(key), upper_bound(key));
}

template <typename K, typename V>
GenericAATree::iterator AATree<K, V>::find(Key key) {
  iterator iter = lower_bound(key);
  if (iter == end() || compare_(key, DoGetKey(*iter))) {
    return end();
  }
  return iter;
}

template <typename K, typename V>
std::pair<GenericAATree::iterator, GenericAATree::iterator>
AATree<K, V>::equal_range(Key key) {
  return std::make_pair(lower_bound(key), upper_bound(key));
}

template <typename K, typename V>
GenericAATree::iterator AATree<K, V>::lower_bound(Key key) {
  AATreeItem* item = GetLowerBoundImpl(root_, key);
  return item == nullptr ? end() : iterator(&root_, item);
}

template <typename K, typename V>
AATreeItem* AATree<K, V>::GetLowerBoundImpl(AATreeItem* item, Key key) {
  if (item == nullptr) {
    return nullptr;
  }
  // If the item's key is less than the key, go right.
  if (compare_(DoGetKey(*item), key)) {
    return GetLowerBoundImpl(item->right_.get(), key);
  }
  // Otherwise the item's key is greater than the key. Try to go left.
  AATreeItem* left = item->left_.get();
  if (left == nullptr) {
    return item;
  }
  AATreeItem* next = GetLowerBoundImpl(left, key);
  return next == nullptr ? item : next;
}

template <typename K, typename V>
GenericAATree::iterator AATree<K, V>::upper_bound(Key key) {
  AATreeItem* item = GetUpperBoundImpl(root_, key);
  return item == nullptr ? end() : iterator(&root_, item);
}

template <typename K, typename V>
AATreeItem* AATree<K, V>::GetUpperBoundImpl(AATreeItem* item, Key key) {
  if (item == nullptr) {
    return nullptr;
  }
  // If the item's key is less than or equal to the key, go right.
  if (!compare_(key, DoGetKey(*item))) {
    return GetUpperBoundImpl(item->right_.get(), key);
  }
  // Otherwise the item's key is greater than the key. Try to go left.
  AATreeItem* left = item->left_.get();
  if (left == nullptr) {
    return item;
  }
  AATreeItem* next = GetUpperBoundImpl(left, key);
  return next == nullptr ? item : next;
}

}  // namespace pw::containers::internal
