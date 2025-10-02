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

#include "pw_multibuf/internal/chunk_iterator.h"

namespace pw::multibuf {

// Forward declaration for friending.
namespace test {
class IteratorTest;
}  // namespace test

namespace internal {
class GenericMultiBuf;

/// Base class for ranges of chunks.
template <typename Derived, typename Deque>
class ChunksImpl {
 public:
  using size_type = typename Deque::size_type;
  using value_type = typename Deque::value_type;
  using difference_type = typename Deque::difference_type;
  using iterator = ChunkIterator<size_type, /*kIsConst=*/false>;
  using const_iterator = ChunkIterator<size_type, /*kIsConst=*/true>;

  constexpr ChunksImpl() = default;

  constexpr size_type size() const { return deque().size() / depth(); }
  constexpr size_type capacity() const { return deque().capacity() / depth(); }

  constexpr const_iterator cbegin() const { return derived().begin(); }
  constexpr const_iterator cend() const { return derived().end(); }

 protected:
  constexpr void Init(Deque& deque, size_type depth) {
    derived().begin_.deque_ = &deque;
    derived().begin_.depth_ = depth;
    derived().end_.deque_ = &deque;
    derived().end_.depth_ = depth;
    derived().end_.index_ = deque.size();
  }

  constexpr Derived& derived() { return static_cast<Derived&>(*this); }
  constexpr const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }

  constexpr const Deque& deque() const { return *(derived().begin_.deque_); }
  constexpr size_type depth() const { return derived().begin_.depth_; }
};

}  // namespace internal

/// Helper class that allows iterating over read-only chunks in a MultiBuf.
///
/// This allows using range-based for-loops, e.g.
///
/// @code{.cpp}
/// for (ConstByteSpan chunk : multibuf.ConstChunks()) {
///   ReadChunk(chunk);
/// }
/// @endcode
///
/// @warning Modifying the structure of a MultiBuf invalidates any outstanding
/// chunk iterators.
template <typename Deque>
class Chunks : public internal::ChunksImpl<Chunks<Deque>, Deque> {
 private:
  using Base = internal::ChunksImpl<Chunks<Deque>, Deque>;

 public:
  using typename Base::const_iterator;
  using typename Base::difference_type;
  using typename Base::iterator;
  using typename Base::size_type;
  using typename Base::value_type;

  constexpr Chunks() = default;

  constexpr iterator begin() const { return begin_; }
  constexpr iterator end() const { return end_; }

 private:
  template <typename, typename>
  friend class ::pw::multibuf::internal::ChunksImpl;
  friend class ::pw::multibuf::internal::GenericMultiBuf;

  // For unit testing.
  friend class ::pw::multibuf::test::IteratorTest;

  constexpr Chunks(Deque& deque, size_type depth) { Base::Init(deque, depth); }

  iterator begin_;
  iterator end_;
};

/// Helper class that allows iterating over mutable chunks in a MultiBuf.
///
/// This allows using range-based for-loops, e.g.
///
/// @code{.cpp}
/// for (ByteSpan chunk : multibuf.ConstChunks()) {
///   ModifyChunk(chunk);
/// }
/// @endcode
///
/// @warning Modifying the structure of a MultiBuf invalidates any outstanding
/// chunk iterators.
template <typename Deque>
class ConstChunks
    : public internal::ChunksImpl<ConstChunks<Deque>, const Deque> {
 private:
  using Base = internal::ChunksImpl<ConstChunks<Deque>, const Deque>;

 public:
  using typename Base::const_iterator;
  using typename Base::difference_type;
  using typename Base::size_type;
  using typename Base::value_type;

  constexpr ConstChunks() = default;

  constexpr const_iterator begin() const { return begin_; }
  constexpr const_iterator end() const { return end_; }

 private:
  template <typename, typename>
  friend class ::pw::multibuf::internal::ChunksImpl;
  friend class ::pw::multibuf::internal::GenericMultiBuf;

  constexpr ConstChunks(const Deque& deque, size_type depth) {
    Base::Init(deque, depth);
  }

  const_iterator begin_;
  const_iterator end_;
};

}  // namespace pw::multibuf
