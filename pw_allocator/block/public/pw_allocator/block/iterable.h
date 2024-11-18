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

#include "pw_allocator/block/contiguous.h"

namespace pw::allocator {
namespace internal {

// Trivial base classes for trait support.
struct ForwardIterableBase {};
struct ReverseIterableBase {};

}  // namespace internal

/// Mix-in for blocks that allows creating forward iterators over block ranges.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `ContiguousBlock`.
template <typename Derived>
class ForwardIterableBlock : public internal::ForwardIterableBase {
 protected:
  constexpr explicit ForwardIterableBlock() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_contiguous_v<Derived>,
                  "Types derived from ForwardIterableBlock must also derive "
                  "from ContiguousBlock");
  }

 public:
  /// Represents an iterator that moves forward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::Range`.
  ///
  /// Allocating or freeing blocks invalidates the iterator.
  class Iterator final {
   public:
    constexpr explicit Iterator(Derived* block) : block_(block) {}
    constexpr Iterator& operator++() {
      block_ = block_ != nullptr ? block_->Next() : nullptr;
      return *this;
    }
    constexpr bool operator!=(const Iterator& other) {
      return block_ != other.block_;
    }
    constexpr Derived* operator*() { return block_; }

   private:
    Derived* block_;
  };

  /// Represents a range of blocks that can be iterated over.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : Range(first, last)) { ... }
  /// @endcode
  ///
  /// Allocating or freeing blocks invalidates the range.
  class Range final {
   public:
    /// Constructs a range including `begin` and all valid following blocks.
    constexpr explicit Range(Derived* begin) : begin_(begin), end_(nullptr) {}

    /// Constructs a range of blocks from `begin` to `end`, inclusively.
    constexpr Range(Derived* begin_inclusive, Derived* end_inclusive)
        : begin_(begin_inclusive), end_(end_inclusive->Next()) {}

    constexpr Iterator& begin() { return begin_; }
    constexpr Iterator& end() { return end_; }

   private:
    Iterator begin_;
    Iterator end_;
  };
};

/// Mix-in for blocks that allows creating reverse iterators over block ranges.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `ContiguousBlock`.
template <typename Derived>
class ReverseIterableBlock : public internal::ReverseIterableBase {
 protected:
  constexpr explicit ReverseIterableBlock() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_contiguous_v<Derived>,
                  "Types derived from ReverseIterableBlock must also derive "
                  "from ContiguousBlock");
  }

 public:
  /// Represents an iterator that moves backward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::ReverseRange`.
  ///
  /// Allocating or freeing blocks invalidates the iterator.
  class ReverseIterator final {
   public:
    constexpr ReverseIterator(Derived* block) : block_(block) {}
    constexpr ReverseIterator& operator++() {
      block_ = block_ != nullptr ? block_->Prev() : nullptr;
      return *this;
    }
    constexpr bool operator!=(const ReverseIterator& other) {
      return block_ != other.block_;
    }
    constexpr Derived* operator*() { return block_; }

   private:
    Derived* block_;
  };

  /// Represents a range of blocks that can be iterated over in the reverse
  /// direction.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : ReverseRange(last, first)) { ... }
  /// @endcode
  ///
  /// Allocating or freeing blocks invalidates the range.
  class ReverseRange final {
   public:
    /// Constructs a range including `rbegin` and all valid preceding blocks.
    constexpr explicit ReverseRange(Derived* rbegin)
        : begin_(rbegin), end_(nullptr) {}

    /// Constructs a range of blocks from `rbegin` to `rend`, inclusively.
    constexpr ReverseRange(Derived* rbegin_inclusive, Derived* rend_inclusive)
        : begin_(rbegin_inclusive), end_(rend_inclusive->Prev()) {}

    constexpr ReverseIterator& begin() { return begin_; }
    constexpr ReverseIterator& end() { return end_; }

   private:
    ReverseIterator begin_;
    ReverseIterator end_;
  };
};

/// Trait type that allow interrogating a block as to whether it is forward-
/// iterable.
template <typename BlockType>
struct is_forward_iterable
    : std::is_base_of<internal::ForwardIterableBase, BlockType> {};

/// Helper variable template for `is_forward_iterable<BlockType>::value`.
template <typename BlockType>
inline constexpr bool is_forward_iterable_v =
    is_forward_iterable<BlockType>::value;

/// Trait type that allow interrogating a block as to whether it is reverse-
/// iterable.
template <typename BlockType>
struct is_reverse_iterable
    : std::is_base_of<internal::ReverseIterableBase, BlockType> {};

/// Helper variable template for `is_reverse_iterable<BlockType>::value`.
template <typename BlockType>
inline constexpr bool is_reverse_iterable_v =
    is_reverse_iterable<BlockType>::value;

}  // namespace pw::allocator
