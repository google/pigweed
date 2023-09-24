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

#include "pw_allocator_private/allocator_testing.h"

#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator::test {

static Block* NextBlock(Block* block) {
  return block->Last() ? nullptr : block->Next();
}

Status FakeAllocator::Initialize(ByteSpan buffer) {
  if (auto status = Block::Init(buffer, &head_); !status.ok()) {
    return status;
  }
  ResetParameters();
  return OkStatus();
}

void FakeAllocator::Exhaust() {
  for (Block* block = head_; block != nullptr; block = NextBlock(block)) {
    block->MarkUsed();
  }
}

void FakeAllocator::ResetParameters() {
  allocate_size_ = 0;
  deallocate_ptr_ = nullptr;
  deallocate_size_ = 0;
  resize_ptr_ = nullptr;
  resize_old_size_ = 0;
  resize_new_size_ = 0;
}

Status FakeAllocator::DoQuery(const void* ptr, size_t size, size_t) const {
  PW_CHECK(head_ != nullptr);
  if (size == 0 || ptr == nullptr) {
    return Status::OutOfRange();
  }
  const auto* bytes = static_cast<const std::byte*>(ptr);
  Block* block = Block::FromUsableSpace(const_cast<std::byte*>(bytes));
  if (!block->IsValid()) {
    return Status::OutOfRange();
  }
  size = AlignUp(size, alignof(Block));
  for (Block* curr = head_; curr != nullptr; curr = NextBlock(curr)) {
    if (curr == block && curr->InnerSize() == size) {
      return OkStatus();
    }
  }
  return Status::OutOfRange();
}

void* FakeAllocator::DoAllocate(size_t size, size_t) {
  PW_CHECK(head_ != nullptr);
  allocate_size_ = size;
  for (Block* block = head_; block != nullptr; block = NextBlock(block)) {
    Block* fragment = nullptr;
    if (!block->Used() && block->Split(size, &fragment).ok()) {
      block->MarkUsed();
      return block->UsableSpace();
    }
  }
  return nullptr;
}

void FakeAllocator::DoDeallocate(void* ptr, size_t size, size_t alignment) {
  PW_CHECK(head_ != nullptr);
  deallocate_ptr_ = ptr;
  deallocate_size_ = size;
  if (!DoQuery(ptr, size, alignment).ok()) {
    return;
  }
  Block* block = Block::FromUsableSpace(static_cast<std::byte*>(ptr));
  block->MarkFree();
  block->MergeNext().IgnoreError();
  block->MergePrev().IgnoreError();
}

bool FakeAllocator::DoResize(void* ptr,
                             size_t old_size,
                             size_t old_alignment,
                             size_t new_size) {
  PW_CHECK(head_ != nullptr);
  resize_ptr_ = ptr;
  resize_old_size_ = old_size;
  resize_new_size_ = new_size;
  if (!DoQuery(ptr, old_size, old_alignment).ok() || old_size == 0 ||
      new_size == 0) {
    return false;
  }
  if (old_size == new_size) {
    return true;
  }
  Block* block = Block::FromUsableSpace(static_cast<std::byte*>(ptr));
  block->MarkFree();
  block->MergeNext().IgnoreError();
  Block* fragment = nullptr;
  if (block->Split(new_size, &fragment) == Status::OutOfRange()) {
    block->Split(old_size, &fragment).IgnoreError();
  }
  block->MarkUsed();
  return block->InnerSize() >= new_size;
}

}  // namespace pw::allocator::test
