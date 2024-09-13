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
#pragma once

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pw_allocator/allocator.h"
#include "pw_allocator/buffer.h"
#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::allocator {
namespace internal {

// Types of corrupted blocks, and functions to crash with an error message
// corresponding to each type. These functions are implemented independent of
// any template parameters to allow them to use `PW_CHECK`.
enum class BlockStatus {
  kValid,
  kMisaligned,
  kPrevMismatched,
  kNextMismatched,
  kPoisonCorrupted,
};
void CrashMisaligned(uintptr_t addr);
void CrashNextMismatched(uintptr_t addr, uintptr_t next_prev);
void CrashPrevMismatched(uintptr_t addr, uintptr_t prev_next);
void CrashPoisonCorrupted(uintptr_t addr);

}  // namespace internal

/// Describes the side effects of fulfilling and allocation request.
enum class BlockAllocType {
  /// The allocation fit exactly, and no other blocks were affected.
  kExact,

  /// Extra trailing space was split off into a new block.
  kNewNext,

  /// Extra leading space was split off into a new block.
  kNewPrev,

  /// Extra leading and trailing space was split off into new blocks.
  kNewPrevAndNewNext,

  /// A small amount of leading space was shifted to the end of the previous
  /// block.
  kShiftToPrev,

  /// A small amount of leading space was shifted to the end of the previous
  /// block, and extra trailing space was split off into a new block.
  kShiftToPrevAndNewNext,
};

/// Memory region with links to adjacent blocks.
///
/// The blocks do not encode their size directly. Instead, they encode offsets
/// to the next and previous blocks using the type given by the `OffsetType`
/// template parameter. The encoded offsets are simply the offsets divided by
/// the minimum block alignment, `kAlignment`.
///
/// `kAlignment` is set by the `kAlign` template parameter, which defaults to
/// its minimum value of `alignof(OffsetType)`. Since the addressable range of a
/// block is given by `std::numeric_limits<OffsetType>::max() * kAlignment`, it
/// may be advantageous to set a higher alignment if it allows using a smaller
/// offset type, even if this wastes some bytes in order to align block headers.
///
/// Blocks will always be aligned to a `kAlignment` boundary. Block sizes will
/// always be rounded up to a multiple of `kAlignment`.
///
/// If `kCanPoison` is set, allocators may call `Poison` to overwrite the
/// contents of a block with a poison pattern. This pattern will subsequently be
/// checked when allocating blocks, and can detect memory corruptions such as
/// use-after-frees.
///
/// As an example, the diagram below represents two contiguous
/// `Block<uint32_t, 4, true>`s. The indices indicate byte offsets:
///
/// @code{.unparsed}
/// Block 1:
/// +---------------------+------+--------------+
/// | Header              | Info | Usable space |
/// +----------+----------+------+--------------+
/// | Prev     | Next     |      |              |
/// | 0......3 | 4......7 | 8..9 | 10.......280 |
/// | 00000000 | 00000046 | 8004 |  <app data>  |
/// +----------+----------+------+--------------+
/// Block 2:
/// +---------------------+------+--------------+
/// | Header              | Info | Usable space |
/// +----------+----------+------+--------------+
/// | Prev     | Next     |      |              |
/// | 0......3 | 4......7 | 8..9 | 10......1056 |
/// | 00000046 | 00000106 | 6004 | f7f7....f7f7 |
/// +----------+----------+------+--------------+
/// @endcode
///
/// The overall size of the block (e.g. 280 bytes) is given by its next offset
/// multiplied by the alignment (e.g. 0x46 * 4). Also, the next offset of a
/// block matches the previous offset of its next block. The first block in a
/// list is denoted by having a previous offset of `0`.
///
/// @tparam   OffsetType  Unsigned integral type used to encode offsets. Larger
///                       types can address more memory, but consume greater
///                       overhead.
/// @tparam   kAlign      Sets the overall alignment for blocks. Minimum is
///                       `alignof(OffsetType)` (the default). Larger values can
///                       address more memory, but consume greater overhead.
/// @tparam   kCanPoison  Indicates whether to enable poisoning free blocks.
template <typename OffsetType_ = uintptr_t,
          size_t kAlign = alignof(OffsetType_),
          bool kCanPoison = false>
class Block {
 public:
  using OffsetType = OffsetType_;
  static_assert(std::is_unsigned_v<OffsetType>, "offset type must be unsigned");

  static constexpr size_t kAlignment = std::max(kAlign, alignof(OffsetType));
  static constexpr size_t kBlockOverhead = AlignUp(sizeof(Block), kAlignment);

  // No copy or move.
  Block(const Block& other) = delete;
  Block& operator=(const Block& other) = delete;

  /// @brief Creates the first block for a given memory region.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Returns a block representing the region.
  ///
  ///    INVALID_ARGUMENT: The region is null.
  ///
  ///    RESOURCE_EXHAUSTED: The region is too small for a block.
  ///
  ///    OUT_OF_RANGE: The region is too big to be addressed using
  ///    ``OffsetType``.
  ///
  /// @endrst
  static Result<Block*> Init(ByteSpan region);

  /// @returns  A pointer to a `Block`, given a pointer to the start of the
  ///           usable space inside the block.
  ///
  /// This is the inverse of `UsableSpace()`.
  ///
  /// @warning  This method does not do any checking; passing a random
  ///           pointer will return a non-null pointer.
  template <int&... DeducedTypesOnly,
            typename PtrType,
            bool is_const_ptr = std::is_const_v<std::remove_pointer_t<PtrType>>,
            typename BytesPtr =
                std::conditional_t<is_const_ptr, const std::byte*, std::byte*>,
            typename BlockPtr =
                std::conditional_t<is_const_ptr, const Block*, Block*>>
  static BlockPtr FromUsableSpace(PtrType usable_space) {
    // Perform memory laundering to prevent the compiler from tracing the memory
    // used to store the block and to avoid optimaztions that may be invalidated
    // by the use of placement-new to create blocks in `Init` and `Split`.
    auto* bytes = reinterpret_cast<BytesPtr>(usable_space);
    return std::launder(reinterpret_cast<BlockPtr>(bytes - kBlockOverhead));
  }

  /// @returns The total size of the block in bytes, including the header.
  size_t OuterSize() const { return next_ * kAlignment; }

  /// @returns The number of usable bytes inside the block.
  size_t InnerSize() const { return OuterSize() - kBlockOverhead; }

  /// @returns The number of bytes requested using AllocFirst, AllocLast, or
  /// Resize.
  size_t RequestedSize() const { return InnerSize() - padding_; }

  /// @returns A pointer to the usable space inside this block.
  std::byte* UsableSpace() {
    return std::launder(reinterpret_cast<std::byte*>(this) + kBlockOverhead);
  }
  const std::byte* UsableSpace() const {
    return std::launder(reinterpret_cast<const std::byte*>(this) +
                        kBlockOverhead);
  }

  /// Splits an aligned block from the start of the block, and marks it as used.
  ///
  /// If successful, `block` will be replaced by a block that has an inner
  /// size of at least `inner_size`, and whose starting address is aligned to an
  /// `alignment` boundary. If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block. In total, up to two
  /// additional blocks may be created: one to pad the returned block to an
  /// alignment boundary and one for the trailing space.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The split completed successfully.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    OUT_OF_RANGE: The requested size plus padding needed for alignment
  ///    is greater than the current size.
  ///
  /// @endrst
  static Result<BlockAllocType> AllocFirst(Block*& block, Layout layout);

  /// Checks if an aligned block could be split from the end of the block.
  ///
  /// On error, this method will return the same status as `AllocLast` without
  /// performing any modifications.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Returns the number of bytes from this block that would precede an
  ///    a block allocated from this one and aligned according to `layout`.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    OUT_OF_RANGE: The requested size is greater than the current size.
  ///
  ///    RESOURCE_EXHAUSTED: The remaining space is too small to hold a
  ///    new block.
  ///
  /// @endrst
  StatusWithSize CanAllocLast(Layout layout) const;

  /// Splits an aligned block from the end of the block, and marks it as used.
  ///
  /// If successful, `block` will be replaced by a block that has an inner
  /// size of at least `inner_size`, and whose starting address is aligned to an
  /// `alignment` boundary. If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block. An additional block may
  /// be created for the leading space.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The split completed successfully.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    OUT_OF_RANGE: The requested size is greater than the current size.
  ///
  ///    RESOURCE_EXHAUSTED: The remaining space is too small to hold a new
  ///    block.
  ///
  /// @endrst
  static Result<BlockAllocType> AllocLast(Block*& block, Layout layout);

  /// Marks the block as free and merges it with any free neighbors.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer. If neither member is free, the returned pointer will point to the
  /// original block. Otherwise, it will point to the new, larger block created
  /// by merging adjacent free blocks together.
  static void Free(Block*& block);

  /// Grows or shrinks the block.
  ///
  /// If successful, `block` may be merged with the block after it in order to
  /// provide additional memory (when growing) or to merge released memory (when
  /// shrinking). If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block.
  ///
  /// @pre The block must be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The resize completed successfully.
  ///
  ///    FAILED_PRECONDITION: This block is not in use.
  ///
  ///    OUT_OF_RANGE: The requested size is greater than the available space.
  ///
  /// @endrst
  static Status Resize(Block*& block, size_t new_inner_size);

  /// Fetches the block immediately after this one.
  ///
  /// For performance, this always returns a block pointer, even if the returned
  /// pointer is invalid. The pointer is valid if and only if `Last()` is false.
  ///
  /// Typically, after calling `Init` callers may save a pointer past the end of
  /// the list using `Next()`. This makes it easy to subsequently iterate over
  /// the list:
  /// @code{.cpp}
  ///   auto result = Block<>::Init(byte_span);
  ///   Block<>* begin = *result;
  ///   Block<>* end = begin->Next();
  ///   ...
  ///   for (auto* block = begin; block != end; block = block->Next()) {
  ///     // Do something which each block.
  ///   }
  /// @endcode
  Block* Next() const;

  /// @copydoc `Next`.
  static Block* NextBlock(const Block* block) {
    return block == nullptr ? nullptr : block->Next();
  }

  /// @returns The block immediately before this one, or a null pointer if this
  /// is the first block.
  Block* Prev() const;

  /// @copydoc `Prev`.
  static Block* PrevBlock(const Block* block) {
    return block == nullptr ? nullptr : block->Prev();
  }

  /// Returns the current alignment of a block.
  size_t Alignment() const { return Used() ? info_.alignment : kAlignment; }

  /// Indicates whether the block is in use.
  ///
  /// @returns `true` if the block is in use or `false` if not.
  bool Used() const { return info_.used; }

  /// Indicates whether this block is the last block or not (i.e. whether
  /// `Next()` points to a valid block or not). This is needed because
  /// `Next()` points to the end of this block, whether there is a valid
  /// block there or not.
  ///
  /// @returns `true` is this is the last block or `false` if not.
  bool Last() const { return info_.last; }

  /// Marks this block as in use.
  void MarkUsed() { info_.used = 1; }

  /// Marks this block as free.
  void MarkFree() { info_.used = 0; }

  /// Marks this block as the last one in the chain.
  void MarkLast() { info_.last = 1; }

  /// Clears the last bit from this block.
  void ClearLast() { info_.last = 0; }

  /// Poisons the block's usable space.
  ///
  /// This method does nothing if `kCanPoison` is false, or if the block is in
  /// use, or if `should_poison` is false. The decision to poison a block is
  /// deferred to the allocator to allow for more nuanced strategies than simply
  /// all or nothing. For example, an allocator may want to balance security and
  /// performance by only poisoning every n-th free block.
  ///
  /// @param  should_poison   Indicates tha block should be poisoned, if
  ///                         poisoning is enabled.
  void Poison(bool should_poison = true);

  /// @brief Checks if a block is valid.
  ///
  /// @returns `true` if and only if the following conditions are met:
  /// * The block is aligned.
  /// * The prev/next fields match with the previous and next blocks.
  /// * The poisoned bytes are not damaged (if poisoning is enabled).
  bool IsValid() const {
    return CheckStatus() == internal::BlockStatus::kValid;
  }

  /// @brief Crashes with an informtaional message if a block is invalid.
  ///
  /// Does nothing if the block is valid.
  void CrashIfInvalid() const;

 private:
  static constexpr uint8_t kPoisonByte = 0xf7;

  /// Consumes the block and returns as a span of bytes.
  static ByteSpan AsBytes(Block*&& block);

  /// Consumes the span of bytes and uses it to construct and return a block.
  static Block* AsBlock(size_t prev_outer_size, ByteSpan bytes);

  Block(size_t prev_outer_size, size_t outer_size);

  /// Returns a `BlockStatus` that is either kValid or indicates the reason why
  /// the block is invalid.
  ///
  /// If the block is invalid at multiple points, this function will only return
  /// one of the reasons.
  internal::BlockStatus CheckStatus() const;

  /// Consumes the given number of leading bytes of a block in order to align
  /// its usable space.
  ///
  /// If the number of bytes needed to align the usable space is 0, i.e. it is
  /// already aligned, this method does nothing. If the number of bytes is less
  /// than the overhead for a block, the bytes are added to the end of the
  /// preceding block, if there is one. Otherwise, if it is the first block or
  /// the number of bytes is greater than the overhead for a block, a new block
  /// is split from this one.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block.
  ///
  /// @pre The block must not be in use.
  static BlockAllocType ShiftBlock(Block*& block, size_t pad_size);

  /// Split a block into two smaller blocks.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block with an inner size of
  /// `new_inner_size`, rounded up to a `kAlignment` boundary. The remaining
  /// space will be returned as a new block.
  ///
  /// @pre The block must not be in use.
  /// @pre The block must have enough usable space for the requested size.
  /// @pre The space remaining after a split can hold a new block.
  static Block* Split(Block*& block, size_t new_inner_size);

  /// Merges this block with next block if it exists and is free; otherwise does
  /// nothing.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, larger block.
  ///
  /// @pre The blocks must not be in use.
  static void MergeNext(Block*& block);

  /// Returns true if the block is unpoisoned or if its usable space is
  /// untouched; false otherwise.
  bool CheckPoison() const;

  /// Offset (in increments of the minimum alignment) from this block to the
  /// previous block. 0 if this is the first block.
  OffsetType prev_ = 0;

  /// Offset (in increments of the minimum alignment) from this block to the
  /// next block. Valid even if this is the last block, since it equals the
  /// size of the block.
  OffsetType next_ = 0;

  /// Information about the current state of the block:
  /// * If the `used` flag is set, the block's usable memory has been allocated
  ///   and is being used.
  /// * If the `poisoned` flag is set and the `used` flag is clear, the block's
  ///   usable memory contains a poison pattern that will be checked when the
  ///   block is allocated.
  /// * If the `last` flag is set, the block does not have a next block.
  /// * If the `used` flag is set, the alignment represents the requested value
  ///   when the memory was allocated, which may be less strict than the actual
  ///   alignment.
  struct {
    uint16_t used : 1;
    uint16_t poisoned : 1;
    uint16_t last : 1;
    uint16_t alignment : 13;
  } info_;

  /// Number of bytes allocated beyond what was requested. This will be at most
  /// the minimum alignment, i.e. `alignof(OffsetType).`
  uint16_t padding_ = 0;

 public:
  // Associated types.

  /// Represents an iterator that moves forward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::Range`.
  class Iterator final {
   public:
    Iterator(Block* block) : block_(block) {}
    Iterator& operator++() {
      block_ = Block::NextBlock(block_);
      return *this;
    }
    bool operator!=(const Iterator& other) { return block_ != other.block_; }
    Block* operator*() { return block_; }

   private:
    Block* block_;
  };

  /// Represents an iterator that moves forward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::ReverseRange`.
  class ReverseIterator final {
   public:
    ReverseIterator(Block* block) : block_(block) {}
    ReverseIterator& operator++() {
      block_ = Block::PrevBlock(block_);
      return *this;
    }
    bool operator!=(const ReverseIterator& other) {
      return block_ != other.block_;
    }
    Block* operator*() { return block_; }

   private:
    Block* block_;
  };

  /// Represents a range of blocks that can be iterated over.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : Range(first, last)) { ... }
  /// @endcode
  class Range final {
   public:
    /// Constructs a range including `begin` and all valid following blocks.
    explicit Range(Block* begin) : begin_(begin), end_(nullptr) {}

    /// Constructs a range of blocks from `begin` to `end`, inclusively.
    Range(Block* begin_inclusive, Block* end_inclusive)
        : begin_(begin_inclusive), end_(end_inclusive->Next()) {}

    Iterator& begin() { return begin_; }
    Iterator& end() { return end_; }

   private:
    Iterator begin_;
    Iterator end_;
  };

  /// Represents a range of blocks that can be iterated over in the reverse
  /// direction.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : ReverseRange(last, first)) { ... }
  /// @endcode
  class ReverseRange final {
   public:
    /// Constructs a range including `rbegin` and all valid preceding blocks.
    explicit ReverseRange(Block* rbegin) : begin_(rbegin), end_(nullptr) {}

    /// Constructs a range of blocks from `rbegin` to `rend`, inclusively.
    ReverseRange(Block* rbegin_inclusive, Block* rend_inclusive)
        : begin_(rbegin_inclusive), end_(rend_inclusive->Prev()) {}

    ReverseIterator& begin() { return begin_; }
    ReverseIterator& end() { return end_; }

   private:
    ReverseIterator begin_;
    ReverseIterator end_;
  };
} __attribute__((packed, aligned(kAlign)));

// Public template method implementations.

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Result<Block<OffsetType, kAlign, kCanPoison>*>
Block<OffsetType, kAlign, kCanPoison>::Init(ByteSpan region) {
  Result<ByteSpan> result = GetAlignedSubspan(region, kAlignment);
  if (!result.ok()) {
    return result.status();
  }
  region = result.value();
  if (region.size() < kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  if (std::numeric_limits<OffsetType>::max() < region.size() / kAlignment) {
    return Status::OutOfRange();
  }
  Block* block = AsBlock(0, region);
  block->MarkLast();
  return block;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Result<BlockAllocType> Block<OffsetType, kAlign, kCanPoison>::AllocFirst(
    Block*& block, Layout layout) {
  if (block == nullptr || layout.size() == 0) {
    return Status::InvalidArgument();
  }
  block->CrashIfInvalid();
  if (block->Used()) {
    return Status::FailedPrecondition();
  }
  Block* prev = block->Prev();
  if (block->InnerSize() < layout.size()) {
    return Status::OutOfRange();
  }

  // Check if padding will be needed at the front to align the usable space.
  size_t alignment = std::max(layout.alignment(), kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_size = AlignUp(addr, alignment) - addr;
  bool should_poison = block->info_.poisoned;
  if (pad_size == 0) {
    // No padding needed.
  } else if (pad_size > kBlockOverhead) {
    // Enough padding is needed that it can be split off as a new free block.
  } else if (prev == nullptr) {
    // First block; increase padding to at least the minimum block size.
    pad_size += AlignUp(kBlockOverhead, alignment);
  } else {
    // Less than a block's worth of padding is needed; shift bytes to the
    // previous block.
  }

  // Make sure everything fits.
  size_t inner_size = AlignUp(layout.size(), kAlignment);
  if (block->InnerSize() < pad_size + inner_size) {
    return Status::ResourceExhausted();
  }
  BlockAllocType alloc_type = ShiftBlock(block, pad_size);

  // If the block is large enough to have a trailing block, split it to get the
  // requested usable space.
  if (inner_size + kBlockOverhead <= block->InnerSize()) {
    Block* trailing = Split(block, inner_size);
    trailing->Poison(should_poison);
    switch (alloc_type) {
      case BlockAllocType::kExact:
        alloc_type = BlockAllocType::kNewNext;
        break;
      case BlockAllocType::kNewPrev:
        alloc_type = BlockAllocType::kNewPrevAndNewNext;
        break;
      case BlockAllocType::kShiftToPrev:
        alloc_type = BlockAllocType::kShiftToPrevAndNewNext;
        break;
      case BlockAllocType::kNewNext:
      case BlockAllocType::kNewPrevAndNewNext:
      case BlockAllocType::kShiftToPrevAndNewNext:
      default:
        // `AllocLast` never creates a trailing block.
        PW_CRASH("unreachable");
    }
  }

  block->MarkUsed();
  block->info_.alignment = alignment;
  block->padding_ = block->InnerSize() - layout.size();
  return alloc_type;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
StatusWithSize Block<OffsetType, kAlign, kCanPoison>::CanAllocLast(
    Layout layout) const {
  if (layout.size() == 0) {
    return StatusWithSize::InvalidArgument();
  }
  if (Used()) {
    return StatusWithSize::FailedPrecondition();
  }
  CrashIfInvalid();
  // Find the last address that is aligned and is followed by enough space for
  // block overhead and the requested size.
  if (InnerSize() < layout.size()) {
    return StatusWithSize::OutOfRange();
  }
  auto addr = reinterpret_cast<uintptr_t>(UsableSpace());
  size_t alignment = std::max(layout.alignment(), kAlignment);
  uintptr_t next = AlignDown(addr + (InnerSize() - layout.size()), alignment);

  if (next < addr) {
    // Requested size does not fit.
    return StatusWithSize::ResourceExhausted();
  }
  size_t extra = next - addr;
  if (extra > kBlockOverhead) {
    // Sufficient extra room for a new block.
    return StatusWithSize(extra);
  }
  if (Prev() != nullptr) {
    // Extra can be shifted to the previous block.
    return StatusWithSize(extra);
  }
  if (extra % alignment == 0) {
    // Pad the end of the block.
    return StatusWithSize();
  }
  return StatusWithSize::ResourceExhausted();
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Result<BlockAllocType> Block<OffsetType, kAlign, kCanPoison>::AllocLast(
    Block*& block, Layout layout) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  size_t pad_size = 0;
  PW_TRY_ASSIGN(pad_size, block->CanAllocLast(layout));

  BlockAllocType alloc_type = ShiftBlock(block, pad_size);

  block->MarkUsed();
  block->info_.alignment = layout.alignment();
  block->padding_ = block->InnerSize() - layout.size();
  return alloc_type;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
BlockAllocType Block<OffsetType, kAlign, kCanPoison>::ShiftBlock(
    Block*& block, size_t pad_size) {
  if (pad_size == 0) {
    return BlockAllocType::kExact;
  }

  bool should_poison = block->info_.poisoned;
  Block* prev = block->Prev();
  if (pad_size <= kBlockOverhead) {
    // The small amount of padding can be appended to the previous block.
    Block::Resize(prev, prev->InnerSize() + pad_size).IgnoreError();
    prev->padding_ += pad_size;
    block = prev->Next();
    return BlockAllocType::kShiftToPrev;
  } else {
    // Split the large padding off the front.
    Block* leading = block;
    block = Split(leading, pad_size - kBlockOverhead);
    leading->Poison(should_poison);
    return BlockAllocType::kNewPrev;
  }
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::Free(Block*& block) {
  if (block == nullptr) {
    return;
  }
  block->MarkFree();
  MergeNext(block);
  Block* prev = block->Prev();
  if (prev == nullptr) {
    // First block, nothing prior to merge with.
    return;
  }
  if (!prev->Used()) {
    // Previous block is free; merge with it.
    MergeNext(prev);
    block = prev;
    return;
  }
  if (prev->padding_ >= kAlignment) {
    // Previous block has trailing unused space from `ShiftBlock`. Resizing will
    // automatically add it to the block that has been freed.
    Resize(prev, prev->InnerSize() - prev->padding_).IgnoreError();
    block = prev->Next();
  }
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::Resize(Block*& block,
                                                     size_t new_inner_size) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  if (!block->Used()) {
    return Status::FailedPrecondition();
  }
  size_t requested_inner_size = new_inner_size;
  size_t old_inner_size = block->InnerSize();
  size_t alignment = block->Alignment();
  uint16_t padding = block->padding_;
  new_inner_size = AlignUp(new_inner_size, kAlignment);

  if (old_inner_size == new_inner_size) {
    return OkStatus();
  }

  // Treat the block as free and try to combine it with the next block. At most
  // one free block is expected to follow this block.
  block->MarkFree();
  MergeNext(block);

  Status status = OkStatus();

  if (block->InnerSize() < new_inner_size) {
    // The merged block is too small for the resized block.
    status = Status::OutOfRange();

  } else if (new_inner_size + kBlockOverhead <= block->InnerSize()) {
    // There is enough room after the resized block for another trailing block.
    bool should_poison = block->info_.poisoned;
    Block* trailing = Split(block, new_inner_size);
    trailing->Poison(should_poison);
  }

  if (status.ok()) {
    padding = block->InnerSize() - requested_inner_size;
  } else if (block->InnerSize() != old_inner_size) {
    // Restore the original block on failure.
    Split(block, old_inner_size);
  }
  block->MarkUsed();
  block->info_.alignment = alignment;
  block->padding_ = padding;
  return status;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>*
Block<OffsetType, kAlign, kCanPoison>::Split(Block*& block,
                                             size_t new_inner_size) {
  size_t prev_outer_size = block->prev_ * kAlignment;
  size_t outer_size1 = new_inner_size + kBlockOverhead;
  bool is_last = block->Last();
  ByteSpan bytes = AsBytes(std::move(block));
  Block* block1 = AsBlock(prev_outer_size, bytes.subspan(0, outer_size1));
  Block* block2 = AsBlock(outer_size1, bytes.subspan(outer_size1));
  if (is_last) {
    block2->MarkLast();
  } else {
    block2->Next()->prev_ = block2->next_;
  }
  block = std::move(block1);
  return block2;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::MergeNext(Block*& block) {
  if (block->Last()) {
    return;
  }
  Block* next = block->Next();
  if (block->Used() || next->Used()) {
    return;
  }
  size_t prev_outer_size = block->prev_ * kAlignment;
  bool is_last = next->Last();
  ByteSpan prev_bytes = AsBytes(std::move(block));
  ByteSpan next_bytes = AsBytes(std::move(next));
  size_t outer_size = prev_bytes.size() + next_bytes.size();
  std::byte* merged = ::new (prev_bytes.data()) std::byte[outer_size];
  block = AsBlock(prev_outer_size, ByteSpan(merged, outer_size));
  if (is_last) {
    block->MarkLast();
  } else {
    block->Next()->prev_ = block->next_;
  }
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>*
Block<OffsetType, kAlign, kCanPoison>::Next() const {
  uintptr_t addr = Last() ? 0 : reinterpret_cast<uintptr_t>(this) + OuterSize();
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>*
Block<OffsetType, kAlign, kCanPoison>::Prev() const {
  uintptr_t addr =
      (prev_ == 0) ? 0
                   : reinterpret_cast<uintptr_t>(this) - (prev_ * kAlignment);
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

// Private template method implementations.

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>::Block(size_t prev_outer_size,
                                             size_t outer_size) {
  prev_ = prev_outer_size / kAlignment;
  next_ = outer_size / kAlignment;
  info_.used = 0;
  info_.poisoned = 0;
  info_.last = 0;
  info_.alignment = kAlignment;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
ByteSpan Block<OffsetType, kAlign, kCanPoison>::AsBytes(Block*&& block) {
  size_t block_size = block->OuterSize();
  std::byte* bytes = ::new (std::move(block)) std::byte[block_size];
  return {bytes, block_size};
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>*
Block<OffsetType, kAlign, kCanPoison>::AsBlock(size_t prev_outer_size,
                                               ByteSpan bytes) {
  return ::new (bytes.data()) Block(prev_outer_size, bytes.size());
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::Poison(bool should_poison) {
  if constexpr (kCanPoison) {
    if (!Used() && should_poison) {
      std::memset(UsableSpace(), kPoisonByte, InnerSize());
      info_.poisoned = true;
    }
  }
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
bool Block<OffsetType, kAlign, kCanPoison>::CheckPoison() const {
  if constexpr (kCanPoison) {
    if (!info_.poisoned) {
      return true;
    }
    const std::byte* begin = UsableSpace();
    return std::all_of(begin, begin + InnerSize(), [](std::byte b) {
      return static_cast<uint8_t>(b) == kPoisonByte;
    });
  } else {
    return true;
  }
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
internal::BlockStatus Block<OffsetType, kAlign, kCanPoison>::CheckStatus()
    const {
  if (reinterpret_cast<uintptr_t>(this) % kAlignment != 0) {
    return internal::BlockStatus::kMisaligned;
  }
  if (!Last() && (this >= Next() || this != Next()->Prev())) {
    return internal::BlockStatus::kNextMismatched;
  }
  if (Prev() && (this <= Prev() || this != Prev()->Next())) {
    return internal::BlockStatus::kPrevMismatched;
  }
  if (!Used() && !CheckPoison()) {
    return internal::BlockStatus::kPoisonCorrupted;
  }
  return internal::BlockStatus::kValid;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::CrashIfInvalid() const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(this);
  switch (CheckStatus()) {
    case internal::BlockStatus::kValid:
      break;
    case internal::BlockStatus::kMisaligned:
      internal::CrashMisaligned(addr);
      break;
    case internal::BlockStatus::kNextMismatched:
      internal::CrashNextMismatched(
          addr, reinterpret_cast<uintptr_t>(Next()->Prev()));
      break;
    case internal::BlockStatus::kPrevMismatched:
      internal::CrashPrevMismatched(
          addr, reinterpret_cast<uintptr_t>(Prev()->Next()));
      break;
    case internal::BlockStatus::kPoisonCorrupted:
      internal::CrashPoisonCorrupted(addr);
      break;
  }
}

}  // namespace pw::allocator
