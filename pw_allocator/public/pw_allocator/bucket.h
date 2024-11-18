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
#include <limits>

#include "lib/stdcompat/bit.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::allocator {

/// Doubly linked list of free memory regions, or "chunks", of a maximum size or
/// less.
class Bucket final {
 public:
  /// When part of a `Bucket`, each `Chunk` will contain a pointer to the next
  /// and previous chunks in the bucket.
  struct Chunk {
   private:
    friend class Bucket;

    static Chunk* FromBytes(std::byte* ptr) {
      return std::launder(reinterpret_cast<Chunk*>(ptr));
    }

    const std::byte* AsBytes() const {
      return cpp20::bit_cast<const std::byte*>(this);
    }

    std::byte* AsBytes() { return cpp20::bit_cast<std::byte*>(this); }

    Chunk* prev;
    Chunk* next;
  };

  /// Constructs a bucket with an unbounded chunk size.
  Bucket();

  /// Construct a bucket.
  ///
  /// @param  chunk_size  The maximum size of the memory chunks in this bucket.
  ///                     Must be at least `sizeof(Chunk)`.
  explicit Bucket(size_t chunk_size);

  Bucket(const Bucket& other) = delete;
  Bucket& operator=(const Bucket& other) = delete;

  Bucket(Bucket&& other) { *this = std::move(other); }
  Bucket& operator=(Bucket&& other);

  ~Bucket() = default;

  void Init(size_t chunk_size = std::numeric_limits<size_t>::max());

  /// Creates a list of buckets, with each twice as large as the one before it.
  static void Init(span<Bucket> buckets, size_t min_chunk_size);

  size_t chunk_size() const { return chunk_size_; }

  bool empty() const { return sentinel_.next == &sentinel_; }

  /// Returns the number of the chunks in the bucket.
  ///
  /// Note: This method runs in O(n) time.
  size_t count() const;

  /// Adds a memory region to this bucket.
  ///
  /// @param  ptr   The memory region to be added.
  void Add(std::byte* ptr);

  /// Applies the given function to each chunk in the bucket.
  void Visit(const Function<void(const std::byte*)>& visitor) const;

  /// Removes a chunk from this bucket.
  ///
  /// @retval       The removed region, or null if the bucket is empty.
  std::byte* Remove();

  /// Removes a chunk for which a given condition is met.
  ///
  /// This will remove at most one chunk.
  ///
  /// @retval       The first chunk for which the condition evaluates to true,
  ///               null if the bucket does not contain any such chunk.
  /// @param  cond  The condition to be tested on the chunks in this bucket.
  std::byte* RemoveIf(const Function<bool(const std::byte*)>& cond);

  /// Removes a chunk from any bucket it is a part of.
  ///
  /// @retval       The removed region, for convenience.
  /// @param  ptr   The memory region to be removed.
  static std::byte* Remove(std::byte* ptr);

 private:
  /// Removes a chunk from any bucket it is a part of.
  ///
  /// @retval       The removed region, for convenience.
  /// @param  chunk   The chunk to be removed.
  static std::byte* Remove(Chunk* chunk);

  /// List terminator node that is before the head and after the tail of the
  /// circular list.
  Chunk sentinel_;

  /// The maximum size of chunks in this bucket.
  size_t chunk_size_;
};

}  // namespace pw::allocator
