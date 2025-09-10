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

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_multibuf/internal/entry.h"
#include "pw_preprocessor/compiler.h"

namespace pw::multibuf_impl {

// Forward declarations.
template <typename, typename>
class ChunksImpl;

/// Type for iterating over the chunks added to a multibuf.
///
/// MultiBufs can be thought of as a sequence of "layers", where each layer
/// except the bottommost is comprised of subspans of the layer below it, and
/// the bottommost references the actual memory. This type can be used to
/// retrieve the contiguous byte spans of the topmost layer of a multibuf. It is
/// distinguished from `ByteIterator`, which iterates over individual bytes of
/// the topmost layer.
template <typename SizeType, bool kIsConst>
class ChunkIterator {
 private:
  using SpanType = std::conditional_t<kIsConst, ConstByteSpan, ByteSpan>;
  using ByteType = typename SpanType::element_type;
  using Deque = std::conditional_t<kIsConst,
                                   const DynamicDeque<Entry, SizeType>,
                                   DynamicDeque<Entry, SizeType>>;

 public:
  using size_type = SizeType;
  using difference_type = std::ptrdiff_t;
  using value_type = SpanType;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator_category = std::bidirectional_iterator_tag;

  constexpr ChunkIterator() = default;
  constexpr ChunkIterator(const ChunkIterator& other) { *this = other; }
  constexpr ChunkIterator& operator=(const ChunkIterator& other);

  // Support converting non-const iterators to const_iterators.
  constexpr operator ChunkIterator<SizeType, /*kIsConst=*/true>() const {
    return {deque_, depth_, index_};
  }

  constexpr reference operator*() {
    PW_ASSERT(is_valid());
    return current_;
  }

  constexpr const_reference operator*() const {
    PW_ASSERT(is_valid());
    return current_;
  }

  constexpr pointer operator->() {
    PW_ASSERT(is_valid());
    return &current_;
  }

  constexpr const_pointer operator->() const {
    PW_ASSERT(is_valid());
    return &current_;
  }

  constexpr ChunkIterator& operator++();

  constexpr ChunkIterator operator++(int) {
    ChunkIterator previous(*this);
    operator++();
    return previous;
  }

  constexpr ChunkIterator& operator--();

  constexpr ChunkIterator operator--(int) {
    ChunkIterator previous(*this);
    operator--();
    return previous;
  }

  constexpr friend bool operator==(const ChunkIterator& lhs,
                                   const ChunkIterator& rhs) {
    return lhs.deque_ == rhs.deque_ && lhs.depth_ == rhs.depth_ &&
           lhs.index_ == rhs.index_;
  }

  constexpr friend bool operator!=(const ChunkIterator& lhs,
                                   const ChunkIterator& rhs) {
    return !(lhs == rhs);
  }

 private:
  // Iterators that point to something are created `Chunks` or `ConstChunks`.
  template <typename, typename>
  friend class ChunksImpl;

  // Allow non-const iterators to construct const_iterators in conversions.
  template <typename, bool>
  friend class ChunkIterator;

  // Byte iterators use chunk iterators to get contiguous spans.
  template <typename, bool>
  friend class ByteIterator;

  constexpr ChunkIterator(Deque* deque, size_type depth, size_type index)
      : deque_(deque), depth_(depth), index_(index) {
    ResetCurrent();
  }

  constexpr bool is_valid() const {
    return deque_ != nullptr && index_ < deque_->size();
  }

  constexpr ByteType* data(size_type index) const {
    return (*deque_)[index].data + (*deque_)[index + depth_ - 1].view.offset;
  }

  constexpr size_t size(size_type index) const {
    return (*deque_)[index + depth_ - 1].view.length;
  }

  constexpr void ResetCurrent();

  Deque* deque_ = nullptr;
  size_type depth_ = 0;
  size_type index_ = 0;
  SpanType current_;
};

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
template <typename SizeType = uint16_t>
class Chunks
    : public ChunksImpl<Chunks<SizeType>, DynamicDeque<Entry, SizeType>> {
 private:
  using Deque = DynamicDeque<Entry, SizeType>;
  using Base = ChunksImpl<Chunks<SizeType>, Deque>;

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
  friend class ChunksImpl;
  friend class GenericMultiBuf;

  // For unit testing.
  friend class IteratorTest;

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
template <typename SizeType = uint16_t>
class ConstChunks : public ChunksImpl<ConstChunks<SizeType>,
                                      const DynamicDeque<Entry, SizeType>> {
 private:
  using Deque = const DynamicDeque<Entry, SizeType>;
  using Base = ChunksImpl<ConstChunks<SizeType>, Deque>;

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
  friend class ChunksImpl;
  friend class GenericMultiBuf;

  constexpr ConstChunks(Deque& deque, size_type depth) {
    Base::Init(deque, depth);
  }

  const_iterator begin_;
  const_iterator end_;
};

// Template method implementations.

template <typename SizeType, bool kIsConst>
constexpr ChunkIterator<SizeType, kIsConst>&
ChunkIterator<SizeType, kIsConst>::operator=(const ChunkIterator& other) {
  deque_ = other.deque_;
  depth_ = other.depth_;
  index_ = other.index_;
  ResetCurrent();
  return *this;
}

template <typename SizeType, bool kIsConst>
constexpr ChunkIterator<SizeType, kIsConst>&
ChunkIterator<SizeType, kIsConst>::operator++() {
  PW_ASSERT(is_valid());
  size_t left = current_.size();
  while (left != 0) {
    left -= size(index_);
    index_ += depth_;
  }
  while (index_ < deque_->size() && size(index_) == 0) {
    index_ += depth_;
  }
  ResetCurrent();
  return *this;
}

template <typename SizeType, bool kIsConst>
constexpr ChunkIterator<SizeType, kIsConst>&
ChunkIterator<SizeType, kIsConst>::operator--() {
  PW_ASSERT(deque_ != nullptr);
  PW_ASSERT(index_ != 0);
  current_ = SpanType();
  while (index_ != 0) {
    SpanType prev(data(index_ - depth_), size(index_ - depth_));
    if (!current_.empty() && prev.data() + prev.size() != current_.data()) {
      break;
    }
    current_ = SpanType(prev.data(), prev.size() + current_.size());
    index_ -= depth_;
  }
  return *this;
}

template <typename SizeType, bool kIsConst>
constexpr void ChunkIterator<SizeType, kIsConst>::ResetCurrent() {
  if (!is_valid()) {
    current_ = SpanType();
    return;
  }
  current_ = SpanType(data(index_), size(index_));
  for (size_type i = index_; i < deque_->size() - depth_; i += depth_) {
    SpanType next(data(i + depth_), size(i + depth_));
    if (current_.empty()) {
      current_ = next;
      index_ += depth_;
      continue;
    }
    if (current_.data() + current_.size() != next.data()) {
      break;
    }
    current_ = SpanType(current_.data(), current_.size() + next.size());
  }
}

}  // namespace pw::multibuf_impl
