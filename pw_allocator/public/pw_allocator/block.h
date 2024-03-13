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

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pw_allocator/allocator.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::allocator {
namespace internal {

// Types of corrupted blocks, and functions to crash with an error message
// corresponding to each type. These functions are implemented independent of
// any template parameters to allow them to use `PW_CHECK`.
enum BlockStatus {
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

/// Memory region with links to adjacent blocks.
///
/// The blocks do not encode their size directly. Instead, they encode offsets
/// to the next and previous blocks using the type given by the `OffsetType`
/// template parameter. The encoded offsets are simply the offsets divded by the
/// minimum block alignment, `kAlignment`.
///
/// The `kAlignment` constant provided by the derived block is typically the
/// minimum value of `alignof(OffsetType)`. Since the addressable range of a
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
/// `Block<uint32_t, true, 8>`s. The indices indicate byte offsets:
///
/// @code{.unparsed}
/// Block 1:
/// +---------------------+------+--------------+
/// | Header              | Info | Usable space |
/// +----------+----------+------+--------------+
/// | Prev     | Next     |      |              |
/// | 0......3 | 4......7 | 8..9 | 10.......280 |
/// | 00000000 | 00000046 | 8008 |  <app data>  |
/// +----------+----------+------+--------------+
/// Block 2:
/// +---------------------+------+--------------+
/// | Header              | Info | Usable space |
/// +----------+----------+------+--------------+
/// | Prev     | Next     |      |              |
/// | 0......3 | 4......7 | 8..9 | 10......1056 |
/// | 00000046 | 00000106 | 6008 | f7f7....f7f7 |
/// +----------+----------+------+--------------+
/// @endcode
///
/// The overall size of the block (e.g. 280 bytes) is given by its next offset
/// multiplied by the alignment (e.g. 0x106 * 4). Also, the next offset of a
/// block matches the previous offset of its next block. The first block in a
/// list is denoted by having a previous offset of `0`.
///
/// @tparam   OffsetType  Unsigned integral type used to encode offsets. Larger
///                       types can address more memory, but consume greater
///                       overhead.
/// @tparam   kCanPoison  Indicates whether to enable poisoning free blocks.
/// @tparam   kAlign      Sets the overall alignment for blocks. Minimum is
///                       `alignof(OffsetType)` (the default). Larger values can
///                       address more memory, but consume greater overhead.
template <typename OffsetType = uintptr_t,
          size_t kAlign = alignof(OffsetType),
          bool kCanPoison = false>
class Block {
 public:
  using offset_type = OffsetType;
  static_assert(std::is_unsigned_v<offset_type>,
                "offset type must be unsigned");

  static constexpr size_t kAlignment = std::max(kAlign, alignof(offset_type));
  static constexpr size_t kBlockOverhead = AlignUp(sizeof(Block), kAlignment);

  // No copy or move.
  Block(const Block& other) = delete;
  Block& operator=(const Block& other) = delete;

  /// @brief Creates the first block for a given memory region.
  ///
  /// @retval OK                    Returns a block representing the region.
  /// @retval INVALID_ARGUMENT      The region is null.
  /// @retval RESOURCE_EXHAUSTED    The region is too small for a block.
  /// @retval OUT_OF_RANGE          The region is too big to be addressed using
  ///                               `OffsetType`.
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

  /// @returns A pointer to the usable space inside this block.
  std::byte* UsableSpace() {
    return std::launder(reinterpret_cast<std::byte*>(this) + kBlockOverhead);
  }
  const std::byte* UsableSpace() const {
    return std::launder(reinterpret_cast<const std::byte*>(this) +
                        kBlockOverhead);
  }

  /// Checks if an aligned block could be split from the start of the block.
  ///
  /// This method will return the same status as `AllocFirst` without performing
  /// any modifications.
  ///
  /// @pre The block must not be in use.
  ///
  /// @retval   OK                  The split would complete successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size plus padding needed for
  ///                               alignment is greater than the current size.
  Status CanAllocFirst(size_t inner_size, size_t alignment) const;

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
  /// @retval   OK                  The split completed successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size plus padding needed for
  ///                               alignment is greater than the current size.
  static Status AllocFirst(Block*& block, size_t inner_size, size_t alignment);

  /// Checks if an aligned block could be split from the end of the block.
  ///
  /// This method will return the same status as `AllocLast` without performing
  /// any modifications.
  ///
  /// @pre The block must not be in use.
  ///
  /// @retval   OK                  The split completed successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size is greater than the
  ///                               current size.
  /// @retval   RESOURCE_EXHAUSTED  The remaining space is too small to hold a
  ///                               new block.
  Status CanAllocLast(size_t inner_size, size_t alignment) const;

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
  /// @retval   OK                  The split completed successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size is greater than the
  ///                               current size.
  /// @retval   RESOURCE_EXHAUSTED  The remaining space is too small to hold a
  ///                               new block.
  static Status AllocLast(Block*& block, size_t inner_size, size_t alignment);

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
  /// @retval   OK                  The resize completed successfully.
  /// @retval   FAILED_PRECONDITION This block is not in use.
  /// @retval   OUT_OF_RANGE        The requested size is greater than the
  ///                               available space.
  static Status Resize(Block*& block, size_t new_inner_size);

  /// Attempts to split this block.
  ///
  /// If successful, the block will have an inner size of `new_inner_size`,
  /// rounded up to a `kAlignment` boundary. The remaining space will be
  /// returned as a new block.
  ///
  /// This method may fail if the remaining space is too small to hold a new
  /// block. If this method fails for any reason, the original block is
  /// unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block.
  ///
  /// @pre The block must not be in use.
  ///
  /// @retval   OK                  The split completed successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size for this block is greater
  ///                               than the current `inner_size`.
  /// @retval   RESOURCE_EXHAUSTED  The remaining space is too small to hold a
  ///                               new block.
  static Result<Block*> Split(Block*& block, size_t new_inner_size);

  /// Merges this block with the one that comes after it.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, larger block.
  ///
  /// @pre The blocks must not be in use.
  ///
  /// @retval   OK                  The merge was successful.
  /// @retval   OUT_OF_RANGE        The given block is the last block.
  /// @retval   FAILED_PRECONDITION One or more of the blocks is in use.
  static Status MergeNext(Block*& block);

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

  /// Returns the layout of a used block.
  Result<Layout> GetLayout() const {
    if (!Used()) {
      return Status::FailedPrecondition();
    }
    return Layout(InnerSize(), info_.alignment);
  }

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
  void ClearLast() { info_.last = 1; }

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
  bool IsValid() const { return CheckStatus() == internal::kValid; }

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

  /// Attempts to adjust the parameters for `AllocFirst` to split valid blocks.
  ///
  /// This method will increase `inner_size` and `alignment` to match valid
  /// values for splitting a block, if possible. It will also set the outer size
  /// of a padding block, if needed.
  Status AdjustForAllocFirst(size_t& inner_size,
                             size_t& pad_outer_size,
                             size_t& alignment) const;

  /// Attempts to adjust the parameters for `AllocLast` to split valid blocks.
  ///
  /// This method will increase `inner_size` and `alignment` to match valid
  /// values for splitting a block, if possible. It will also set the outer size
  /// of a padding block, if needed.
  Status AdjustForAllocLast(size_t& inner_size,
                            size_t& pad_outer_size,
                            size_t& alignment) const;

  /// Like `Split`, but assumes the caller has already checked to parameters to
  /// ensure the split will succeed.
  static Block* SplitImpl(Block*& block, size_t new_inner_size);

  /// Returns true if the block is unpoisoned or if its usable space is
  /// untouched; false otherwise.
  bool CheckPoison() const;

  offset_type prev_ = 0;
  offset_type next_ = 0;
  struct {
    uint16_t used : 1;
    uint16_t poisoned : 1;
    uint16_t last : 1;
    uint16_t alignment : 13;
  } info_;

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
};

// Public template method implementations.

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Result<Block<OffsetType, kAlign, kCanPoison>*>
Block<OffsetType, kAlign, kCanPoison>::Init(ByteSpan region) {
  if (region.data() == nullptr) {
    return Status::InvalidArgument();
  }
  auto addr = reinterpret_cast<uintptr_t>(region.data());
  auto aligned = AlignUp(addr, kAlignment);
  if (addr + region.size() <= aligned + kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  region = region.subspan(aligned - addr);
  if (std::numeric_limits<OffsetType>::max() < region.size() / kAlignment) {
    return Status::OutOfRange();
  }
  Block* block = AsBlock(0, region);
  block->MarkLast();
  return block;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::AdjustForAllocFirst(
    size_t& inner_size, size_t& pad_outer_size, size_t& alignment) const {
  if (Used()) {
    return Status::FailedPrecondition();
  }
  CrashIfInvalid();
  // Check if padding will be needed at the front to align the usable space.
  alignment = std::max(alignment, kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(this) + kBlockOverhead;
  if (addr % alignment != 0) {
    pad_outer_size = AlignUp(addr + kBlockOverhead, alignment) - addr;
    inner_size += pad_outer_size;
  } else {
    pad_outer_size = 0;
  }
  inner_size = AlignUp(inner_size, kAlignment);
  if (InnerSize() < inner_size) {
    return Status::OutOfRange();
  }
  return OkStatus();
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::CanAllocFirst(
    size_t inner_size, size_t alignment) const {
  size_t pad_outer_size = 0;
  return AdjustForAllocFirst(inner_size, pad_outer_size, alignment);
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::AllocFirst(Block*& block,
                                                         size_t inner_size,
                                                         size_t alignment) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  size_t pad_outer_size = 0;
  if (auto status =
          block->AdjustForAllocFirst(inner_size, pad_outer_size, alignment);
      !status.ok()) {
    return status;
  }

  // If the block is large enough to have a trailing block, split it to get the
  // requested usable space.
  bool should_poison = block->info_.poisoned;
  if (inner_size + kBlockOverhead <= block->InnerSize()) {
    Block* trailing = SplitImpl(block, inner_size);
    trailing->Poison(should_poison);
  }

  // If present, split the padding off the front.
  if (pad_outer_size != 0) {
    Block* leading = block;
    block = SplitImpl(leading, pad_outer_size - kBlockOverhead);
    leading->Poison(should_poison);
  }

  block->MarkUsed();
  block->info_.alignment = alignment;
  return OkStatus();
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::AdjustForAllocLast(
    size_t& inner_size, size_t& pad_outer_size, size_t& alignment) const {
  if (Used()) {
    return Status::FailedPrecondition();
  }
  CrashIfInvalid();
  // Find the last address that is aligned and is followed by enough space for
  // block overhead and the requested size.
  if (InnerSize() < inner_size) {
    return Status::OutOfRange();
  }
  alignment = std::max(alignment, kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(this) + kBlockOverhead;
  uintptr_t next = AlignDown(addr + (InnerSize() - inner_size), alignment);
  if (next == addr) {
    pad_outer_size = 0;
    return OkStatus();
  }
  if (next < addr + kBlockOverhead) {
    // A split is needed, but no block will fit.
    return Status::ResourceExhausted();
  }
  pad_outer_size = next - addr;
  return OkStatus();
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::CanAllocLast(
    size_t inner_size, size_t alignment) const {
  size_t pad_outer_size = 0;
  return AdjustForAllocLast(inner_size, pad_outer_size, alignment);
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Status Block<OffsetType, kAlign, kCanPoison>::AllocLast(Block*& block,
                                                        size_t inner_size,
                                                        size_t alignment) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  size_t pad_outer_size = 0;
  if (auto status =
          block->AdjustForAllocLast(inner_size, pad_outer_size, alignment);
      !status.ok()) {
    return status;
  }

  // If present, split the padding off the front.
  bool should_poison = block->info_.poisoned;
  if (pad_outer_size != 0) {
    Block* leading = block;
    block = SplitImpl(leading, pad_outer_size - kBlockOverhead);
    leading->Poison(should_poison);
  }
  block->MarkUsed();
  block->info_.alignment = alignment;
  return OkStatus();
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::Free(Block*& block) {
  if (block == nullptr) {
    return;
  }
  block->MarkFree();
  Block* prev = block->Prev();
  if (MergeNext(prev).ok()) {
    block = prev;
  }
  MergeNext(block).IgnoreError();
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
  size_t old_inner_size = block->InnerSize();
  new_inner_size = AlignUp(new_inner_size, kAlignment);
  if (old_inner_size == new_inner_size) {
    return OkStatus();
  }

  // Treat the block as free and try to combine it with the next block. At most
  // one free block is expecte to follow this block.
  block->MarkFree();
  MergeNext(block).IgnoreError();

  Status status = OkStatus();
  if (block->InnerSize() < new_inner_size) {
    // The merged block is too small for the resized block.
    status = Status::OutOfRange();
  } else if (new_inner_size + kBlockOverhead <= block->InnerSize()) {
    // There is enough room after the resized block for another trailing block.
    bool should_poison = block->info_.poisoned;
    Block* trailing = SplitImpl(block, new_inner_size);
    trailing->Poison(should_poison);
  }

  // Restore the original block on failure.
  if (!status.ok() && block->InnerSize() != old_inner_size) {
    SplitImpl(block, old_inner_size);
  }
  block->MarkUsed();
  return status;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Result<Block<OffsetType, kAlign, kCanPoison>*>
Block<OffsetType, kAlign, kCanPoison>::Split(Block*& block,
                                             size_t new_inner_size) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  if (block->Used()) {
    return Status::FailedPrecondition();
  }
  size_t old_inner_size = block->InnerSize();
  new_inner_size = AlignUp(new_inner_size, kAlignment);
  if (old_inner_size < new_inner_size) {
    return Status::OutOfRange();
  }
  if (old_inner_size - new_inner_size < kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  return SplitImpl(block, new_inner_size);
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
Block<OffsetType, kAlign, kCanPoison>*
Block<OffsetType, kAlign, kCanPoison>::SplitImpl(Block*& block,
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
Status Block<OffsetType, kAlign, kCanPoison>::MergeNext(Block*& block) {
  if (block == nullptr) {
    return Status::InvalidArgument();
  }
  if (block->Last()) {
    return Status::OutOfRange();
  }
  Block* next = block->Next();
  if (block->Used() || next->Used()) {
    return Status::FailedPrecondition();
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
  return OkStatus();
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
    return internal::kMisaligned;
  }
  if (!Last() && (this >= Next() || this != Next()->Prev())) {
    return internal::kNextMismatched;
  }
  if (Prev() && (this <= Prev() || this != Prev()->Next())) {
    return internal::kPrevMismatched;
  }
  if (!Used() && !CheckPoison()) {
    return internal::kPoisonCorrupted;
  }
  return internal::kValid;
}

template <typename OffsetType, size_t kAlign, bool kCanPoison>
void Block<OffsetType, kAlign, kCanPoison>::CrashIfInvalid() const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(this);
  switch (CheckStatus()) {
    case internal::kValid:
      break;
    case internal::kMisaligned:
      internal::CrashMisaligned(addr);
      break;
    case internal::kNextMismatched:
      internal::CrashNextMismatched(
          addr, reinterpret_cast<uintptr_t>(Next()->Prev()));
      break;
    case internal::kPrevMismatched:
      internal::CrashPrevMismatched(
          addr, reinterpret_cast<uintptr_t>(Prev()->Next()));
      break;
    case internal::kPoisonCorrupted:
      internal::CrashPoisonCorrupted(addr);
      break;
  }
}

}  // namespace pw::allocator
