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

#include "pw_allocator/allocator_testing.h"

#include "pw_allocator/block.h"
#include "pw_assert/check.h"

namespace pw::allocator::test {

AllocatorForTest::~AllocatorForTest() { PW_DCHECK(!initialized_); }

Status AllocatorForTest::Init(ByteSpan bytes) {
  ResetParameters();
  initialized_ = true;
  return allocator_.Init(bytes);
}

void AllocatorForTest::Exhaust() {
  for (auto* block : allocator_.blocks()) {
    block->MarkUsed();
  }
}

void AllocatorForTest::ResetParameters() {
  allocate_size_ = 0;
  deallocate_ptr_ = nullptr;
  deallocate_size_ = 0;
  resize_ptr_ = nullptr;
  resize_old_size_ = 0;
  resize_new_size_ = 0;
}

void AllocatorForTest::Reset() {
  for (auto* block : allocator_.blocks()) {
    BlockType::Free(block);
  }
  ResetParameters();
  allocator_.Reset();
  initialized_ = false;
}

Status AllocatorForTest::DoQuery(const void* ptr, Layout layout) const {
  return allocator_.Query(ptr, layout);
}

void* AllocatorForTest::DoAllocate(Layout layout) {
  allocate_size_ = layout.size();
  return allocator_.Allocate(layout);
}

void AllocatorForTest::DoDeallocate(void* ptr, Layout layout) {
  deallocate_ptr_ = ptr;
  deallocate_size_ = layout.size();
  return allocator_.Deallocate(ptr, layout);
}

bool AllocatorForTest::DoResize(void* ptr, Layout layout, size_t new_size) {
  resize_ptr_ = ptr;
  resize_old_size_ = layout.size();
  resize_new_size_ = new_size;
  return allocator_.Resize(ptr, layout, new_size);
}

// AllocatorTestHarnessGeneric methods

namespace {

size_t NumBits(size_t n) { return n == 0 ? 1 : cpp20::bit_width(n); }

AllocationRequest GenerateAllocationRequest(random::RandomGenerator& prng,
                                            size_t max_size) {
  AllocationRequest request;
  if (max_size != 0) {
    prng.GetInt(request.size);
    request.size %= max_size;
  }
  uint8_t lshift;
  prng.GetInt(lshift);
  request.alignment = 1U << (lshift % NumBits(request.size));
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
  if (max_size != 0) {
    prng.GetInt(request.new_size);
    request.new_size %= max_size;
  }
  return request;
}

}  // namespace

void AllocatorTestHarnessGeneric::GenerateRequests(
    random::RandomGenerator& prng, size_t max_size, size_t num_requests) {
  for (size_t i = 0; i < num_requests; ++i) {
    AllocatorRequest request;
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
  Reset();
}

namespace {

auto ArbitrarySize(size_t max_size) {
  return fuzzer::InRange<size_t>(0, max_size);
}

auto ArbitraryAlignment(size_t size) {
  auto pow2 = [](size_t n) { return 1U << n; };
  return fuzzer::Map(pow2, fuzzer::InRange<size_t>(0, NumBits(size) - 1));
}

auto ArbitraryIndex() { return fuzzer::Arbitrary<size_t>(); }

fuzzer::Domain<AllocationRequest> ArbitraryAllocationRequest(size_t max_size) {
  auto from_size = [](size_t size) {
    return fuzzer::StructOf<AllocationRequest>(fuzzer::Just(size),
                                               ArbitraryAlignment(size));
  };
  return fuzzer::FlatMap(from_size, ArbitrarySize(max_size));
}

fuzzer::Domain<DeallocationRequest> ArbitraryDeallocationRequest() {
  return fuzzer::StructOf<DeallocationRequest>(ArbitraryIndex());
}

fuzzer::Domain<ReallocationRequest> ArbitraryReallocationRequest(
    size_t max_size) {
  return fuzzer::StructOf<ReallocationRequest>(ArbitraryIndex(),
                                               ArbitrarySize(max_size));
}

}  // namespace

fuzzer::Domain<AllocatorRequest> ArbitraryAllocatorRequest(size_t max_size) {
  return fuzzer::VariantOf(ArbitraryAllocationRequest(max_size),
                           ArbitraryDeallocationRequest(),
                           ArbitraryReallocationRequest(max_size));
}

void AllocatorTestHarnessGeneric::HandleRequests(
    const Vector<AllocatorRequest>& requests) {
  for (const auto& request : requests) {
    HandleRequest(request);
  }
  Reset();
}

// Helper to allow static_assert'ing in constexpr-if branches, e.g. that a
// visitor for a std::variant are exhaustive.
template <typename T>
static constexpr bool not_reached(T&) {
  return false;
}

void AllocatorTestHarnessGeneric::HandleRequest(
    const AllocatorRequest& request) {
  if (allocator_ == nullptr) {
    allocator_ = Init();
    PW_DCHECK_NOTNULL(allocator_);
  }
  std::visit(
      [this](auto&& r) {
        using T = std::decay_t<decltype(r)>;

        if constexpr (std::is_same_v<T, AllocationRequest>) {
          if (allocations_.size() < allocations_.max_size()) {
            Layout layout(r.size, r.alignment);
            void* ptr = allocator_->Allocate(layout);
            if (ptr != nullptr) {
              AddAllocation(ptr, layout);
            }
          }

        } else if constexpr (std::is_same_v<T, DeallocationRequest>) {
          if (!allocations_.empty()) {
            Allocation old = RemoveAllocation(r.index);
            allocator_->Deallocate(old.ptr, old.layout);
          }

        } else if constexpr (std::is_same_v<T, ReallocationRequest>) {
          if (!allocations_.empty()) {
            Allocation old = RemoveAllocation(r.index);
            void* new_ptr =
                allocator_->Reallocate(old.ptr, old.layout, r.new_size);
            if (new_ptr != nullptr) {
              AddAllocation(new_ptr,
                            Layout(r.new_size, old.layout.alignment()));
            }
          }
        } else {
          static_assert(not_reached(r), "unsupported request type!");
        }
      },
      request);
}

void AllocatorTestHarnessGeneric::Reset() {
  if (allocator_ == nullptr) {
    return;
  }
  for (const Allocation& old : allocations_) {
    allocator_->Deallocate(old.ptr, old.layout);
  }
  allocations_.clear();
}

void AllocatorTestHarnessGeneric::AddAllocation(void* ptr, Layout layout) {
  auto* bytes = static_cast<std::byte*>(ptr);
  size_t left = layout.size();

  // Record the request number in the allocated memory to aid in debugging.
  ++num_requests_;
  if (left >= sizeof(num_requests_)) {
    std::memcpy(bytes, &num_requests_, sizeof(num_requests_));
    left -= sizeof(num_requests_);
  }

  // Record the allocation size in the allocated memory to aid in debugging.
  size_t size = layout.size();
  if (left >= sizeof(size)) {
    std::memcpy(bytes, &size, sizeof(size));
    left -= sizeof(size);
  }

  // Fill the remaining memory with data.
  if (left > 0) {
    std::memset(bytes, 0x5A, left);
  }

  allocations_.emplace_back(Allocation{ptr, layout});
}

AllocatorTestHarnessGeneric::Allocation
AllocatorTestHarnessGeneric::RemoveAllocation(size_t index) {
  // Move the target allocation to the back of the list.
  index %= allocations_.size();
  std::swap(allocations_.at(index), allocations_.back());

  // Copy and remove the targeted allocation.
  Allocation old(allocations_.back());
  allocations_.pop_back();
  return old;
}

}  // namespace pw::allocator::test
