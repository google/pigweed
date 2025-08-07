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
#include "pw_allocator/guarded_allocator.h"

#include "lib/stdcompat/bit.h"
#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"
#include "pw_span/span.h"

namespace pw::allocator::internal {
namespace {

using WordSpan = pw::span<size_t>;

constexpr size_t kMagic = static_cast<size_t>(0xDEFACEDC0DE15BADULL);
constexpr size_t kMinPrefixSize = sizeof(size_t) * 2;
constexpr size_t kSuffixSize = sizeof(size_t);

size_t* AsWords(uintptr_t addr) {
  size_t alignment_offset = addr % alignof(size_t);
  PW_CHECK_UINT_EQ(alignment_offset, 0, "address is not word-aligned");
  return reinterpret_cast<size_t*>(addr);
}

size_t* AsWords(void* ptr, size_t offset) {
  return AsWords(cpp20::bit_cast<uintptr_t>(ptr) + offset);
}

size_t AdjustSizeImpl(size_t prefix_size, size_t inner_size) {
  inner_size = AlignUp(inner_size, alignof(size_t));
  // PW_CHECK(pw::CheckedAdd(...))
  PW_CHECK(pw::CheckedIncrement(inner_size, prefix_size),
           "size overflow when adding prefix");
  PW_CHECK(pw::CheckedIncrement(inner_size, kSuffixSize),
           "size overflow when adding suffix");
  return inner_size;
}

WordSpan GetPrefix(void* ptr, size_t alignment) {
  auto addr = cpp20::bit_cast<uintptr_t>(ptr);
  uintptr_t usable = AlignUp(addr + kMinPrefixSize, alignment);
  size_t num_words = (usable - addr) / sizeof(size_t);
  return WordSpan(AsWords(addr), num_words);
}

WordSpan GetPrefix(void* ptr) {
  size_t* words = AsWords(ptr, 0);
  return WordSpan(words, words[0]);
}

WordSpan FindPrefix(void* ptr) {
  size_t* words = AsWords(ptr, 0);
  size_t num_words = words[-2];
  return WordSpan(words - num_words, num_words);
}

WordSpan GetSuffix(void* ptr, size_t size) {
  if (size < kSuffixSize) {
    return WordSpan();
  }
  size_t* words = AsWords(ptr, size - kSuffixSize);
  return WordSpan(words, 1);
}

}  // namespace

Layout GenericGuardedAllocator::AdjustLayout(Layout layout) {
  size_t alignment = std::max(layout.alignment(), alignof(size_t));
  size_t prefix_size = std::max(alignment, kMinPrefixSize);
  size_t inner_size = AdjustSizeImpl(prefix_size, layout.size());
  return Layout(inner_size, alignment);
}

size_t GenericGuardedAllocator::AdjustSize(void* ptr, size_t inner_size) {
  WordSpan prefix = GetPrefix(ptr);
  size_t prefix_size = prefix.size() * sizeof(size_t);
  return AdjustSizeImpl(prefix_size, inner_size);
}

void* GenericGuardedAllocator::GetOriginal(void* ptr) {
  if (ptr == nullptr) {
    return nullptr;
  }
  WordSpan prefix = FindPrefix(ptr);
  return cpp20::bit_cast<void*>(prefix.data());
}

void* GenericGuardedAllocator::AddPrefix(void* ptr, size_t alignment) {
  WordSpan prefix = GetPrefix(ptr, alignment);
  if (prefix.size() > 2) {
    prefix[0] = prefix.size();
  }
  prefix[prefix.size() - 2] = prefix.size();
  prefix[prefix.size() - 1] = kMagic;
  return cpp20::bit_cast<void*>(prefix.data() + prefix.size());
}

void GenericGuardedAllocator::AddSuffix(void* ptr, size_t size) {
  WordSpan suffix = GetSuffix(ptr, size);
  suffix[0] = kMagic;
}

bool GenericGuardedAllocator::CheckPrefixAndSuffix(void* ptr, size_t size) {
  WordSpan prefix = GetPrefix(ptr);
  WordSpan suffix = GetSuffix(ptr, size);
  size_t num_words = prefix.size();

  // Are the prefix and suffix at least the minimum size?
  if (num_words < 2 || suffix.empty()) {
    return false;
  }

  // Does the prefix take up all the space?
  if (num_words >= (size / sizeof(size_t))) {
    return false;
  }

  // If the prefix records the size twice, do they agree?
  if (num_words != 2 && prefix[num_words - 2] != num_words) {
    return false;
  }

  // Are the guard values correct?
  return prefix[num_words - 1] == kMagic && suffix[0] == kMagic;
}

}  // namespace pw::allocator::internal
