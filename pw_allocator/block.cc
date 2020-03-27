// Copyright 2020 The Pigweed Authors
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

#include "pw_allocator/block.h"

namespace pw::allocator {

Status Block::Init(const span<std::byte> region, Block** block) {
  // Ensure the region we're given is aligned and sized accordingly
  if (reinterpret_cast<uintptr_t>(region.data()) % alignof(Block) != 0) {
    return Status::INVALID_ARGUMENT;
  }

  if (region.size() < sizeof(Block)) {
    return Status::INVALID_ARGUMENT;
  }

  union {
    Block* block;
    std::byte* bytes;
  } aliased;
  aliased.bytes = region.data();

  // Make "next" point just past the end of this block; forming a linked list
  // with the following storage. Since the space between this block and the
  // next are implicitly part of the raw data, size can be computed by
  // subtracting the pointers.
  aliased.block->next = reinterpret_cast<Block*>(region.end());
  aliased.block->MarkLast();

  aliased.block->prev = nullptr;
  *block = aliased.block;
  return Status::OK;
}

Status Block::Split(size_t head_block_inner_size, Block** new_block) {
  if (new_block == nullptr) {
    return Status::INVALID_ARGUMENT;
  }

  // Don't split used blocks.
  // TODO: Relax this restriction? Flag to enable/disable this check?
  if (Used()) {
    return Status::FAILED_PRECONDITION;
  }

  // First round the head_block_inner_size up to a alignof(Block) bounary.
  // This ensures that the next block header is aligned accordingly.
  // Alignment must be a power of two, hence align()-1 will return the
  // remainder.
  auto align_bit_mask = alignof(Block) - 1;
  size_t aligned_head_block_inner_size = head_block_inner_size;
  if ((head_block_inner_size & align_bit_mask) != 0) {
    aligned_head_block_inner_size =
        (head_block_inner_size & ~align_bit_mask) + alignof(Block);
  }

  // (1) Are we trying to allocate a head block larger than the current head
  // block? This may happen because of the alignment above.
  if (aligned_head_block_inner_size > InnerSize()) {
    return Status::OUT_OF_RANGE;
  }

  // (2) Does the resulting block have enough space to store the header?
  // TODO: What to do if the returned section is empty (i.e. remaining
  // size == sizeof(Block))?
  if (InnerSize() - aligned_head_block_inner_size < sizeof(Block)) {
    return Status::RESOURCE_EXHAUSTED;
  }

  // Create the new block inside the current one.
  Block* new_next = reinterpret_cast<Block*>(
      // From the current position...
      reinterpret_cast<intptr_t>(this) +
      // skip past the current header...
      sizeof(*this) +
      // into the usable bytes by the new inner size.
      aligned_head_block_inner_size);

  // If we're inserting in the middle, we need to update the current next
  // block to point to what we're inserting
  if (!Last()) {
    NextBlock()->prev = new_next;
  }

  // Copy next verbatim so the next block also gets the "last"-ness
  new_next->next = next;
  new_next->prev = this;

  // Update the current block to point to the new head.
  next = new_next;

  *new_block = next;
  return Status::OK;
}

Status Block::MergeNext() {
  // Anything to merge with?
  if (Last()) {
    return Status::OUT_OF_RANGE;
  }

  // Is this or the next block in use?
  if (Used() || NextBlock()->Used()) {
    return Status::FAILED_PRECONDITION;
  }

  // Simply enough, this block's next pointer becomes the next block's
  // next pointer. We then need to re-wire the "next next" block's prev
  // pointer to point back to us though.
  next = NextBlock()->next;

  // Copying the pointer also copies the "last" status, so this is safe.
  if (!Last()) {
    NextBlock()->prev = this;
  }

  return Status::OK;
}

Status Block::MergePrev() {
  // We can't merge if we have no previous. After that though, merging with
  // the previous block is just MergeNext from the previous block.
  if (prev == nullptr) {
    return Status::OUT_OF_RANGE;
  }

  // WARNING: This class instance will still exist, but technically be invalid
  // after this has been invoked. Be careful when doing anything with `this`
  // After doing the below.
  return prev->MergeNext();
}

}  // namespace pw::allocator
