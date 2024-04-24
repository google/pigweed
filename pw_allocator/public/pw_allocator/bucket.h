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

#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::allocator::internal {

/// List of free chunks of a fixed size.
///
/// A "chunk" is simply a memory region of at least `sizeof(void*)` bytes.
/// When part of a `Bucket`, each chunk will contain a pointer to the next
/// chunk in the bucket.
class Bucket final {
 public:
  /// Constructs a bucket with an unbounded chunk size.
  constexpr Bucket() = default;

  /// Construct a bucket.
  ///
  /// @param  chunk_size  The fixed size of the memory chunks in this bucket.
  ///                     Must be at least `sizeof(std::byte*)`.
  explicit Bucket(size_t chunk_size);

  Bucket(const Bucket& other) = delete;
  Bucket& operator=(const Bucket& other) = delete;

  Bucket(Bucket&& other) { *this = std::move(other); }
  Bucket& operator=(Bucket&& other);

  ~Bucket() = default;

  /// Creates a list of buckets, with each twice as large as the one before it.
  static void Init(span<Bucket> buckets, size_t min_chunk_size);

  size_t chunk_size() const { return chunk_size_; }

  bool empty() const { return chunks_ == nullptr; }

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

  /// Removes a chunk
  std::byte* RemoveIf(const Function<bool(void*)>& cond);

 private:
  std::byte* chunks_ = nullptr;
  size_t chunk_size_ = std::numeric_limits<size_t>::max();
};

}  // namespace pw::allocator::internal
