// Copyright 2023 The Pigweed Authors
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

#include "pw_allocator/test_harness.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <type_traits>

#include "lib/stdcompat/bit.h"
#include "pw_assert/check.h"

namespace pw::allocator::test {
namespace {

// Helper to allow static_assert'ing in constexpr-if branches, e.g. that a
// visitor for a std::variant are exhaustive.
template <typename T>
constexpr bool not_reached(T&) {
  return false;
}

size_t GenerateSize(random::RandomGenerator& prng, size_t max_size) {
  static constexpr size_t kMinSize = sizeof(TestHarness::Allocation);
  size_t size = 0;
  if (kMinSize < max_size) {
    prng.GetInt(size);
    size %= max_size - kMinSize;
  }
  return kMinSize + size;
}

AllocationRequest GenerateAllocationRequest(random::RandomGenerator& prng,
                                            size_t max_size) {
  AllocationRequest request;
  request.size = GenerateSize(prng, max_size);
  uint8_t lshift;
  prng.GetInt(lshift);
  request.alignment = AlignmentFromLShift(lshift, request.size);
  return request;
}

DeallocationRequest GenerateDeallocationRequest(random::RandomGenerator& prng) {
  DeallocationRequest request;
  prng.GetInt(request.index);
  return request;
}

ReallocationRequest GenerateReallocationRequest(random::RandomGenerator& prng,
                                                size_t max_size) {
  ReallocationRequest request;
  prng.GetInt(request.index);
  request.new_size = GenerateSize(prng, max_size);
  return request;
}

}  // namespace

size_t AlignmentFromLShift(size_t lshift, size_t size) {
  constexpr size_t max_bits = (sizeof(size) * CHAR_BIT) - 1;
  size_t num_bits =
      size == 0 ? 1 : std::min(size_t(cpp20::bit_width(size)), max_bits);
  size_t alignment = 1U << (lshift % num_bits);
  return std::max(alignment, alignof(TestHarness::Allocation));
}

void TestHarness::GenerateRequests(random::RandomGenerator& prng,
                                   size_t max_size,
                                   size_t num_requests) {
  for (size_t i = 0; i < num_requests; ++i) {
    GenerateRequest(prng, max_size);
  }
  Reset();
}

void TestHarness::GenerateRequest(random::RandomGenerator& prng,
                                  size_t max_size) {
  Request request;
  size_t request_type;
  prng.GetInt(request_type);
  switch (request_type % 3) {
    case 0:
      request = GenerateAllocationRequest(prng, max_size);
      break;
    case 1:
      request = GenerateDeallocationRequest(prng);
      break;
    case 2:
      request = GenerateReallocationRequest(prng, max_size);
      break;
  }
  HandleRequest(request);
}

void TestHarness::HandleRequests(const Vector<Request>& requests) {
  for (const auto& request : requests) {
    HandleRequest(request);
  }
  Reset();
}

void TestHarness::HandleRequest(const Request& request) {
  if (allocator_ == nullptr) {
    allocator_ = Init();
    PW_DCHECK_NOTNULL(allocator_);
  }
  std::visit(
      [this](auto&& r) {
        using T = std::decay_t<decltype(r)>;

        if constexpr (std::is_same_v<T, AllocationRequest>) {
          Layout layout(std::max(r.size, sizeof(Allocation)), r.alignment);
          void* ptr = allocator_->Allocate(layout);
          if (ptr != nullptr) {
            AddAllocation(ptr, layout);
          }

        } else if constexpr (std::is_same_v<T, DeallocationRequest>) {
          Allocation* old = RemoveAllocation(r.index);
          if (old == nullptr) {
            return;
          }
          allocator_->Deallocate(old);

        } else if constexpr (std::is_same_v<T, ReallocationRequest>) {
          Allocation* old = RemoveAllocation(r.index);
          if (old == nullptr) {
            return;
          }
          Layout new_layout = Layout(r.new_size, old->layout.alignment());
          void* new_ptr = allocator_->Reallocate(old, new_layout);
          if (new_ptr == nullptr) {
            AddAllocation(old, old->layout);
          } else {
            AddAllocation(new_ptr, new_layout);
          }
        } else {
          static_assert(not_reached(r), "unsupported request type!");
        }
      },
      request);
}

void TestHarness::Reset() {
  if (allocator_ == nullptr) {
    return;
  }
  while (!allocations_.empty()) {
    allocator_->Deallocate(RemoveAllocation(0));
  }
}

TestHarness::Allocation::Allocation(size_t index_, Layout layout_)
    : IntrusiveList<Allocation>::Item(), index(index_), layout(layout_) {}

void TestHarness::AddAllocation(void* ptr, Layout layout) {
  PW_ASSERT(layout.size() >= sizeof(Allocation));
  auto* bytes = static_cast<std::byte*>(ptr);
  ++num_requests_;
  auto* allocation = ::new (bytes) Allocation(num_requests_, layout);
  allocations_.push_back(*allocation);
  ++num_allocations_;
}

TestHarness::Allocation* TestHarness::RemoveAllocation(size_t index) {
  // Move the target allocation to the back of the list.
  if (num_allocations_ == 0) {
    return nullptr;
  }
  index %= num_allocations_;
  --num_allocations_;
  auto prev = allocations_.before_begin();
  for (auto& allocation : allocations_) {
    if (index == 0) {
      allocations_.erase_after(prev);
      return &allocation;
    }
    --index;
    ++prev;
  }
  PW_CRASH("unreachable");
}

}  // namespace pw::allocator::test
