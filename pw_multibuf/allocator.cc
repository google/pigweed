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

#include "pw_multibuf/allocator.h"

#include "pw_assert/check.h"

namespace pw::multibuf {

std::optional<MultiBuf> MultiBufAllocator::Allocate(size_t size) {
  return Allocate(size, size);
}

std::optional<MultiBuf> MultiBufAllocator::Allocate(size_t min_size,
                                                    size_t desired_size) {
  pw::Result<MultiBuf> result = DoAllocate(min_size, desired_size, false);
  if (result.ok()) {
    return std::move(*result);
  }
  return std::nullopt;
}

std::optional<MultiBuf> MultiBufAllocator::AllocateContiguous(size_t size) {
  return AllocateContiguous(size, size);
}

std::optional<MultiBuf> MultiBufAllocator::AllocateContiguous(
    size_t min_size, size_t desired_size) {
  pw::Result<MultiBuf> result = DoAllocate(min_size, desired_size, true);
  if (result.ok()) {
    return std::move(*result);
  }
  return std::nullopt;
}

MultiBufAllocationFuture MultiBufAllocator::AllocateAsync(size_t size) {
  return MultiBufAllocationFuture(*this, size, size, false);
}
MultiBufAllocationFuture MultiBufAllocator::AllocateAsync(size_t min_size,
                                                          size_t desired_size) {
  return MultiBufAllocationFuture(*this, min_size, desired_size, false);
}
MultiBufAllocationFuture MultiBufAllocator::AllocateContiguousAsync(
    size_t size) {
  return MultiBufAllocationFuture(*this, size, size, true);
}
MultiBufAllocationFuture MultiBufAllocator::AllocateContiguousAsync(
    size_t min_size, size_t desired_size) {
  return MultiBufAllocationFuture(*this, min_size, desired_size, true);
}

void MultiBufAllocator::MoreMemoryAvailable(size_t size_available,
                                            size_t contiguous_size_available)
    // Disable lock safety analysis: the access to `next_` requires locking
    // `waiter->allocator_->lock_`, but that's the same as `lock_` which we
    // already hold.
    PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(lock_);
  internal::AllocationWaiter* current = first_waiter_;
  while (current != nullptr) {
    PW_DCHECK(current->allocator_ == this);
    if ((current->min_size_ <= contiguous_size_available) ||
        (!current->needs_contiguous_ && current->min_size_ <= size_available)) {
      std::move(current->waker_).Wake();
    }
    current = current->next_;
  }
}

void MultiBufAllocator::AddWaiter(internal::AllocationWaiter* waiter) {
  std::lock_guard lock(lock_);
  AddWaiterLocked(waiter);
}

void MultiBufAllocator::AddWaiterLocked(internal::AllocationWaiter* waiter)
    // Disable lock safety analysis: the access to `next_` requires locking
    // `waiter->allocator_->lock_`, but that's the same as `lock_` which we
    // already hold.
    PW_NO_LOCK_SAFETY_ANALYSIS {
  PW_DCHECK(waiter->allocator_ == this);
  PW_DCHECK(waiter->next_ == nullptr);
  auto old_first = first_waiter_;
  first_waiter_ = waiter;
  waiter->next_ = old_first;
}

void MultiBufAllocator::RemoveWaiter(internal::AllocationWaiter* waiter)
    // Disable lock safety analysis: the access to `next_` requires locking
    // `waiter->allocator_->lock_`, but that's the same as `lock_` which we
    // already hold.
    PW_NO_LOCK_SAFETY_ANALYSIS {
  std::lock_guard lock(lock_);
  RemoveWaiterLocked(waiter);
  PW_DCHECK(waiter->next_ == nullptr);
}

void MultiBufAllocator::RemoveWaiterLocked(internal::AllocationWaiter* waiter)
    // Disable lock safety analysis: the access to `next_` requires locking
    // `waiter->allocator_->lock_`, but that's the same as `lock_` which we
    // already hold.
    PW_NO_LOCK_SAFETY_ANALYSIS {
  PW_DCHECK(waiter->allocator_ == this);
  if (waiter == first_waiter_) {
    first_waiter_ = first_waiter_->next_;
    waiter->next_ = nullptr;
    return;
  }
  auto current = first_waiter_;
  while (current != nullptr) {
    if (current->next_ == waiter) {
      current->next_ = waiter->next_;
      waiter->next_ = nullptr;
      return;
    }
    current = current->next_;
  }
}

namespace internal {

AllocationWaiter::AllocationWaiter(AllocationWaiter&& other)
    : allocator_(other.allocator_),
      waker_(),
      next_(nullptr),
      min_size_(other.min_size_),
      desired_size_(other.desired_size_),
      needs_contiguous_(other.needs_contiguous_) {
  std::lock_guard lock(allocator_->lock_);
  allocator_->RemoveWaiterLocked(&other);
  allocator_->AddWaiterLocked(this);
  // We must move the waker under the lock in order to ensure that there is no
  // race between swapping ``AllocationWaiter``s and the waker being awoken by
  // the allocator.
  waker_ = std::move(other.waker_);
}

AllocationWaiter& AllocationWaiter::operator=(AllocationWaiter&& other) {
  allocator_->RemoveWaiter(this);

  allocator_ = other.allocator_;
  min_size_ = other.min_size_;
  desired_size_ = other.desired_size_;
  needs_contiguous_ = other.needs_contiguous_;

  std::lock_guard lock(allocator_->lock_);
  allocator_->RemoveWaiterLocked(&other);
  allocator_->AddWaiterLocked(this);
  // We must move the waker under the lock in order to ensure that there is no
  // race between swapping ``AllocationWaiter``s and the waker being awoken by
  // the allocator.
  waker_ = std::move(other.waker_);

  return *this;
}

AllocationWaiter::~AllocationWaiter() { allocator_->RemoveWaiter(this); }

}  // namespace internal

async2::Poll<std::optional<MultiBuf>> MultiBufAllocationFuture::Pend(
    async2::Context& cx) {
  bool should_attempt = waiter_.ShouldAttemptAllocate();
  // We set the waker prior to attempting allocation because we want
  // to receive notifications that may have raced with our attempt
  // to allocate. If we wait to set until after attempting,
  // we'd have to re-attempt in order to ensure that no new memory
  // became available between our attempt and setting the waker.
  waiter_.SetWaker(cx.GetWaker(async2::WaitReason::Unspecified()));
  if (!should_attempt) {
    return async2::Pending();
  }
  auto result = TryAllocate();
  if (result.IsReady()) {
    waiter_.ClearWaker();
  }
  return result;
}

async2::Poll<std::optional<MultiBuf>> MultiBufAllocationFuture::TryAllocate() {
  pw::Result<MultiBuf> buf_opt = waiter_.allocator().DoAllocate(
      waiter_.min_size(), waiter_.desired_size(), waiter_.needs_contiguous());
  if (buf_opt.ok()) {
    return async2::Ready<std::optional<MultiBuf>>(std::move(*buf_opt));
  }
  if (buf_opt.status().IsOutOfRange()) {
    return async2::Ready<std::optional<MultiBuf>>(std::nullopt);
  }
  return async2::Pending();
}

}  // namespace pw::multibuf
