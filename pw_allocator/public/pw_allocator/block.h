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

#include <cstdint>
#include <cstring>

#include "lib/stdcompat/bit.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Representation-independent base class of Block.
///
/// This class contains static methods which do not depend on the template
/// parameters of ``Block`` that are used to encode block information. This
/// reduces the amount of code generated for ``Block``s with different
/// parameters.
///
/// This class should not be used directly. Instead, see ``Block``.
class BaseBlock {
 public:
#if defined(PW_ALLOCATOR_POISON_ENABLE) && PW_ALLOCATOR_POISON_ENABLE
  // Add poison offset of 8 bytes before and after usable space in all
  // Blocks.
  static constexpr size_t kPoisonOffset = 8;
#else
  // Set the poison offset to 0 bytes; will not add poison space before and
  // after usable space in all Blocks.
  static constexpr size_t kPoisonOffset = 0;
#endif  // PW_ALLOCATOR_POISON_ENABLE

  // No copy/move
  BaseBlock(const BaseBlock& other) = delete;
  BaseBlock& operator=(const BaseBlock& other) = delete;
  BaseBlock(BaseBlock&& other) = delete;
  BaseBlock& operator=(BaseBlock&& other) = delete;

 protected:
  enum BlockStatus {
    kValid,
    kMisaligned,
    kPrevMismatched,
    kNextMismatched,
    kPoisonCorrupted,
  };

#if defined(PW_ALLOCATOR_POISON_ENABLE) && PW_ALLOCATOR_POISON_ENABLE
  static constexpr std::byte kPoisonPattern[kPoisonOffset] = {
      std::byte{0x92},
      std::byte{0x88},
      std::byte{0x0a},
      std::byte{0x00},
      std::byte{0xec},
      std::byte{0xdc},
      std::byte{0xae},
      std::byte{0x4e},
  };
#endif  // PW_ALLOCATOR_POISON_ENABLE

  BaseBlock() = default;

  /// Poisons the block's guard regions, if poisoning is enabled.
  ///
  /// Does nothing if poisoning is disabled.
  static void Poison(void* block, size_t header_size, size_t outer_size);

  /// Returns whether the block's guard regions are untouched, if poisoning is
  /// enabled.
  ///
  /// Trivially returns true if poisoning is disabled.
  static bool CheckPoison(const void* block,
                          size_t header_size,
                          size_t outer_size);

  static void CrashMisaligned(uintptr_t addr);
  static void CrashNextMismatched(uintptr_t addr, uintptr_t next_prev);
  static void CrashPrevMismatched(uintptr_t addr, uintptr_t prev_next);
  static void CrashPoisonCorrupted(uintptr_t addr);

  // Associated types

  /// Iterator for a list of blocks.
  ///
  /// This class is templated both on the concrete block type, as well as on a
  /// function that can advance the iterator to the next element. This class
  /// cannot be instantiated directly. Instead, use the `begin` and `end`
  /// methods of `Block::Range` or `Block::ReverseRange`.
  template <typename BlockType, BlockType* (*Advance)(const BlockType*)>
  class BaseIterator {
   public:
    BaseIterator& operator++() {
      if (block_ != nullptr) {
        block_ = Advance(block_);
      }
      return *this;
    }

    bool operator!=(const BaseIterator& other) {
      return block_ != other.block_;
    }

    BlockType* operator*() { return block_; }

   protected:
    BaseIterator(BlockType* block) : block_(block) {}

   private:
    BlockType* block_;
  };

  /// Represents a range of blocks in a list.
  ///
  /// This class is templated both on the concrete block and iterator types.
  /// This class cannot be instantiated directly. Instead, use `Block::Range` or
  /// `Block::ReverseRange`.
  template <typename BlockType, typename IteratorType>
  class BaseRange {
   public:
    IteratorType& begin() { return begin_; }
    IteratorType& end() { return end_; }

   protected:
    BaseRange(BlockType* begin_inclusive, BlockType* end_exclusive)
        : begin_(begin_inclusive), end_(end_exclusive) {}

   private:
    IteratorType begin_;
    IteratorType end_;
  };
};

/// @brief Represents a region of memory as an element of a doubly linked list.
///
/// Typically, an application will start with a single block representing a
/// contiguous region of memory returned from a call to `Init`. This block can
/// be split into smaller blocks that refer to their neighbors. Neighboring
/// blocks can be merged. These behaviors allows ``Allocator``s to track
/// allocated memory with a small amount of overhead.
///
/// For example, the following is a simple but functional ``Allocator`` using
/// ``Block``:
///
/// @code{.cpp}
/// // TODO(b/306686936): Consider moving this to a standalone example.
/// class SimpleAllocator {
/// public:
///   Status Init(ByteSpan region) {
///     auto result = Block<>::Init(region);
///     if (!result.ok()) { return result.status(); }
///     begin_ = *result;
///     end_ = begin_->Next();
///     return OkStatus();
///   }
///
/// private:
///   void* DoAllocate(Layout layout) override {
///     for (auto* block = begin_; block != end_; block = block->Next()) {
///       if (block->InnerSize() >= layout.size()) {
///         if (auto result=Block<>::Split(block, layout.size()); result.ok()) {
///           // Try to merge the leftovers with the next block.
///           Block<>::MergeNext(*result).IgnoreError();
///         }
///         block->MarkUsed();
///         return block->UsableSpace();
///      }
///     }
///     return nullptr;
///   }
///
///   void DoDeallocate(void* ptr, Layout) override {
///     Block<>* block = Block<>::FromUsableSpace(ptr);
///     block->MarkFree();
///     // Try to merge the released block with its neighbors.
///     Block<>::MergeNext(block).IgnoreError();
///     block = block->Prev();
///     Block<>::MergeNext(block).IgnoreError();
///   }
///
///   bool DoResize(void*, Layout, size_t) {
///     // Always reallocate.
///     return false;
///   }
/// };
/// @endcode
///
/// Blocks will always be aligned to a `kAlignment boundary. Block sizes will
/// always be rounded up to a multiple of `kAlignment`.
///
/// The blocks do not encode their size. Instead, they encode the offsets to the
/// next and previous blocks. These offsets are encoded using the type given by
/// the template parameter `T`. The encoded offsets are simply the offsets
/// divded by the minimum alignment.
///
/// Optionally, callers may add guard regions to block by defining
/// `PW_ALLOCATOR_POISON_ENABLE`. These guard regions will be set to a known
/// whenever a block is created and checked when that block is merged. This can
/// catch heap overflows where consumers write beyond the end of the usable
/// space.
///
/// As an example, the diagram below represents two contiguous
/// `Block<uint32_t, ...>`s with heap poisoning enabled and
/// `alignof(uint32_t) == 4`. The indices indicate byte offsets.
///
/// @code{.unparsed}
/// Block 1:
/// +--------------------------------------+----------------+----------------+
/// | Header                               | <Usable space> | Footer         |
/// +----------+----------+----------------+----------------+----------------+
/// | Prev     | Next     |                |                |                |
/// | 0....3   | 4......7 | 8...........15 | 16.........271 | 272........280 |
/// | 00000000 | 00000046 | kPoisonPattern | <Usable space> | kPoisonPattern |
/// +----------+----------+----------------+----------------+----------------+
///
/// Block 2:
/// +--------------------------------------+----------------+----------------+
/// | Header                               | <Usable space> | Footer         |
/// +----------+----------+----------------+----------------+----------------+
/// | Prev     | Next     |                |                |                |
/// | 0....3   | 4......7 | 8...........15 | 16........1039 | 1040......1056 |
/// | 00000046 | 00000106 | kPoisonPattern | <Usable space> | kPoisonPattern |
/// +----------+----------+----------------+----------------+----------------+
/// @endcode
///
/// The overall size of the block (e.g. 280 bytes) is given by its next offset
/// multiplied by the alignment (e.g. 0x106 * 4). Also, the next offset of a
/// block matches the previous offset of its next block. The first block in a
/// list is denoted by having a previous offset of `0`.
///
/// Each block also encodes flags. Builtin flags indicate whether the block is
/// in use and whether it is the last block in the list. The last block will
/// still have a next offset that denotes its size.
///
/// Depending on `kMaxSize`, some bits of type `T` may not be needed to
/// encode an offset. Additional bits of both the previous and next offsets may
/// be used for setting custom flags.
///
/// For example, for a `Block<uint32_t, 0x10000>`, on a platform where
/// `alignof(uint32_t) == 4`, the fully encoded bits would be:
///
/// @code{.unparsed}
/// +-------------------------------------------------------------------------+
/// | block:                                                                  |
/// +------------------------------------+------------------------------------+
/// | .prev_                             | .next_:                            |
/// +---------------+------+-------------+---------------+------+-------------+
/// | MSB           |      |         LSB | MSB           |      |         LSB |
/// | 31.........16 |  15  | 14........0 | 31.........16 |  15  | 14........0 |
/// | custom_flags1 | used | prev_offset | custom_flags2 | last | next_offset |
/// +---------------+------+-------------+---------------+------+-------------+
/// @endcode
///
/// @tparam   UintType  Unsigned integral type used to encode offsets and flags.
/// @tparam   kMaxSize  Largest offset that can be addressed by this block. Bits
///                     of `UintType` not needed for offsets are available as
///                     flags.
template <typename UintType = uintptr_t,
          size_t kMaxSize = std::numeric_limits<uintptr_t>::max()>
class Block final : public BaseBlock {
 public:
  static_assert(std::is_unsigned_v<UintType>);
  static_assert(kMaxSize <= std::numeric_limits<UintType>::max());

  static constexpr size_t kCapacity = kMaxSize;
  static constexpr size_t kHeaderSize = sizeof(Block) + kPoisonOffset;
  static constexpr size_t kFooterSize = kPoisonOffset;
  static constexpr size_t kBlockOverhead = kHeaderSize + kFooterSize;
  static constexpr size_t kAlignment = alignof(Block);

  /// @brief Creates the first block for a given memory region.
  ///
  /// @pre The start of the given memory region must be aligned to an
  /// `kAlignment` boundary.
  ///
  /// @retval OK                    Returns a block representing the region.
  /// @retval INVALID_ARGUMENT      The region is unaligned.
  /// @retval RESOURCE_EXHAUSTED    The region is too small for a block.
  /// @retval OUT_OF_RANGE          The region is larger than `kMaxSize`.
  static Result<Block*> Init(ByteSpan region);

  /// @returns  A pointer to a `Block`, given a pointer to the start of the
  ///           usable space inside the block.
  ///
  /// This is the inverse of `UsableSpace()`.
  ///
  /// @warning  This method does not do any checking; passing a random
  ///           pointer will return a non-null pointer.
  static Block* FromUsableSpace(std::byte* usable_space) {
    // Perform memory laundering to prevent the compiler from tracing the memory
    // used to store the block and to avoid optimaztions that may be invalidated
    // by the use of placement-new to create blocks in `Init` and `Split`.
    return std::launder(reinterpret_cast<Block*>(usable_space - kHeaderSize));
  }

  /// @returns The total size of the block in bytes, including the header.
  size_t OuterSize() const { return GetOffset(next_); }

  /// @returns The number of usable bytes inside the block.
  size_t InnerSize() const { return OuterSize() - kBlockOverhead; }

  /// @returns A pointer to the usable space inside this block.
  std::byte* UsableSpace() {
    // Accessing a dynamic type through a glvalue of std::byte is always well-
    // defined to allow for object representation.
    return reinterpret_cast<std::byte*>(this) + kHeaderSize;
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
  /// @retval   OK                  The split completed successfully.
  /// @retval   FAILED_PRECONDITION This block is in use and cannot be split.
  /// @retval   OUT_OF_RANGE        The requested size plus padding needed for
  ///                               alignment is greater than the current size.
  static Status AllocFirst(Block*& block, size_t inner_size, size_t alignment);

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
  /// @pre The block must not be in use.v
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
  static Block* NextBlock(const Block* block) { return block->Next(); }

  /// @returns The block immediately before this one, or a null pointer if this
  /// is the first block.
  Block* Prev() const;

  /// @copydoc `Prev`.
  static Block* PrevBlock(const Block* block) { return block->Prev(); }

  /// Indicates whether the block is in use.
  ///
  /// @returns `true` if the block is in use or `false` if not.
  bool Used() const { return (prev_ & kBuiltinFlag) != 0; }

  /// Indicates whether this block is the last block or not (i.e. whether
  /// `Next()` points to a valid block or not). This is needed because
  /// `Next()` points to the end of this block, whether there is a valid
  /// block there or not.
  ///
  /// @returns `true` is this is the last block or `false` if not.
  bool Last() const { return (next_ & kBuiltinFlag) != 0; }

  /// Marks this block as in use.
  void MarkUsed() { prev_ |= kBuiltinFlag; }

  /// Marks this block as free.
  void MarkFree() { prev_ &= ~kBuiltinFlag; }

  /// Marks this block as the last one in the chain.
  void MarkLast() { next_ |= kBuiltinFlag; }

  /// Clears the last bit from this block.
  void ClearLast() { next_ &= ~kBuiltinFlag; }

  /// Sets (and clears) custom flags for this block.
  ///
  /// The number of bits available for custom flags depends on the capacity of
  /// the block, and is given by `kCustomFlagBits`. Only this many of the least
  /// significant bits of `flags_to_set` and `flags_to_clear` are considered;
  /// any others are ignored. Refer to the class level documentation for the
  /// exact bit layout.
  ///
  /// Custom flags are not copied when a block is split, and are unchanged when
  /// merging for the block that remains valid after the merge.
  ///
  /// If `flags_to_clear` are provided, these bits will be cleared before
  /// setting the `flags_to_set`. As a consequence, if a bit is set in both
  /// `flags_to_set` and `flags_to_clear`, it will be set upon return.
  ///
  /// @param[in]  flags_to_set      Bit flags to enable.
  /// @param[in]  flags_to_clear    Bit flags to disable.
  void SetFlags(UintType flags_to_set, UintType flags_to_clear = 0);

  /// Returns the custom flags previously set on this block.
  UintType GetFlags();

  /// @brief Checks if a block is valid.
  ///
  /// @returns `true` if and only if the following conditions are met:
  /// * The block is aligned.
  /// * The prev/next fields match with the previous and next blocks.
  /// * The poisoned bytes are not damaged (if poisoning is enabled).
  bool IsValid() const { return CheckStatus() == BlockStatus::kValid; }

  /// @brief Crashes with an informtaional message if a block is invalid.
  ///
  /// Does nothing if the block is valid.
  void CrashIfInvalid();

 private:
  static constexpr UintType kMaxOffset = UintType(kMaxSize / kAlignment);
  static constexpr size_t kCustomFlagBitsPerField =
      cpp20::countl_zero(kMaxOffset) - 1;
  static constexpr size_t kCustomFlagBits = kCustomFlagBitsPerField * 2;
  static constexpr size_t kOffsetBits = cpp20::bit_width(kMaxOffset);
  static constexpr UintType kBuiltinFlag = UintType(1) << kOffsetBits;
  static constexpr UintType kOffsetMask = kBuiltinFlag - 1;
  static constexpr size_t kCustomFlagShift = kOffsetBits + 1;
  static constexpr UintType kCustomFlagMask = ~(kOffsetMask | kBuiltinFlag);

  Block(size_t prev_offset, size_t next_offset);

  /// Consumes the block and returns as a span of bytes.
  static ByteSpan AsBytes(Block*&& block);

  /// Consumes the span of bytes and uses it to construct and return a block.
  static Block* AsBlock(size_t prev_offset, ByteSpan bytes);

  /// Returns a `BlockStatus` that is either kValid or indicates the reason why
  /// the block is invalid.
  ///
  /// If the block is invalid at multiple points, this function will only return
  /// one of the reasons.
  BlockStatus CheckStatus() const;

  /// Extracts the offset portion from `next_` or `prev_`.
  static size_t GetOffset(UintType packed) {
    return static_cast<size_t>(packed & kOffsetMask) * kAlignment;
  }

  /// Overwrites the offset portion of `next_` or `prev_`.
  static void SetOffset(UintType& field, size_t offset) {
    field = (field & ~kOffsetMask) | static_cast<UintType>(offset) / kAlignment;
  }

  UintType next_ = 0;
  UintType prev_ = 0;

 public:
  // Associated types.

  /// Represents an iterator that moves forward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::Range`.
  class Iterator : public BaseIterator<Block, NextBlock> {
   public:
    Iterator(Block* block) : BaseIterator<Block, NextBlock>(block) {}
  };

  /// Represents an iterator that moves forward through a list of blocks.
  ///
  /// This class is not typically instantiated directly, but rather using a
  /// range-based for-loop using `Block::ReverseRange`.
  class ReverseIterator : public BaseIterator<Block, PrevBlock> {
   public:
    ReverseIterator(Block* block) : BaseIterator<Block, PrevBlock>(block) {}
  };

  /// Represents a range of blocks that can be iterated over.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : Range(first, last)) { ... }
  /// @endcode
  class Range : public BaseRange<Block, Iterator> {
   public:
    /// Constructs a range including `begin` and all valid following blocks.
    explicit Range(Block* begin) : BaseRange<Block, Iterator>(begin, nullptr) {}

    /// Constructs a range of blocks from `begin` to `end`, inclusively.
    Range(Block* begin_inclusive, Block* end_inclusive)
        : BaseRange<Block, Iterator>(begin_inclusive, end_inclusive->Next()) {}
  };

  /// Represents a range of blocks that can be iterated over in the reverse
  /// direction.
  ///
  /// The typical usage of this class is in a range-based for-loop, e.g.
  /// @code{.cpp}
  ///   for (auto* block : ReverseRange(last, first)) { ... }
  /// @endcode
  class ReverseRange : public BaseRange<Block, ReverseIterator> {
   public:
    /// Constructs a range including `rbegin` and all valid preceding blocks.
    explicit ReverseRange(Block* rbegin)
        : BaseRange<Block, ReverseIterator>(rbegin, nullptr) {}

    /// Constructs a range of blocks from `rbegin` to `rend`, inclusively.
    ReverseRange(Block* rbegin_inclusive, Block* rend_inclusive)
        : BaseRange<Block, ReverseIterator>(rbegin_inclusive,
                                            rend_inclusive->Prev()) {}
  };
};

// Public template method implementations.

template <typename UintType, size_t kMaxSize>
Result<Block<UintType, kMaxSize>*> Block<UintType, kMaxSize>::Init(
    ByteSpan region) {
  if (reinterpret_cast<uintptr_t>(region.data()) % kAlignment != 0) {
    return Status::InvalidArgument();
  }
  if (region.size() < kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  if (kMaxSize < region.size()) {
    return Status::OutOfRange();
  }
  Block* block = AsBlock(0, region);
  block->MarkLast();
  BaseBlock::Poison(block, kHeaderSize, block->OuterSize());
  return block;
}

template <typename UintType, size_t kMaxSize>
Status Block<UintType, kMaxSize>::AllocFirst(Block*& block,
                                             size_t inner_size,
                                             size_t alignment) {
  // Check if padding will be needed at the front to align the usable space.
  size_t pad_outer_size = 0;
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  if (addr % alignment != 0) {
    pad_outer_size = AlignUp(addr + kBlockOverhead, alignment) - addr;
    inner_size += pad_outer_size;
  }

  // Split the block to get the requested usable space. It is not an error if
  // the block is too small to split off a new trailing block.
  Result<Block*> result = Block::Split(block, inner_size);
  if (!result.ok() && result.status() != Status::ResourceExhausted()) {
    return result.status();
  }

  // If present, split the padding off the front. Since this space was included
  // in the previous split, it should always succeed.
  if (pad_outer_size != 0) {
    result = Block::Split(block, pad_outer_size - kBlockOverhead);
    block = *result;
  }

  block->MarkUsed();
  return OkStatus();
}

template <typename UintType, size_t kMaxSize>
Status Block<UintType, kMaxSize>::AllocLast(Block*& block,
                                            size_t inner_size,
                                            size_t alignment) {
  // Find the last address that is aligned and is followed by enough space for
  // block overhead and the requested size.
  if (block->InnerSize() < inner_size) {
    return Status::OutOfRange();
  }
  alignment = std::max(alignment, kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  uintptr_t next =
      AlignDown(addr + (block->InnerSize() - inner_size), alignment);
  if (next != addr) {
    if (next < addr + kBlockOverhead) {
      // A split is needed, but no block will fit.
      return Status::ResourceExhausted();
    }
    size_t pad_inner_size = next - (addr + kBlockOverhead);
    Result<Block*> result = Block::Split(block, pad_inner_size);
    if (!result.ok()) {
      return result.status();
    }
    block = *result;
  }
  block->MarkUsed();
  return OkStatus();
}

template <typename UintType, size_t kMaxSize>
void Block<UintType, kMaxSize>::Free(Block*& block) {
  block->MarkFree();
  Block* prev = block->Prev();
  if (Block::MergeNext(prev).ok()) {
    block = prev;
  }
  Block::MergeNext(block).IgnoreError();
}

template <typename UintType, size_t kMaxSize>
Status Block<UintType, kMaxSize>::Resize(Block*& block, size_t new_inner_size) {
  if (!block->Used()) {
    return Status::FailedPrecondition();
  }
  size_t old_inner_size = block->InnerSize();
  size_t aligned_inner_size = AlignUp(new_inner_size, kAlignment);
  if (old_inner_size == aligned_inner_size) {
    return OkStatus();
  }

  // Treat the block as free and try to combine it with the next block. At most
  // one free block is expecte to follow this block.
  block->MarkFree();
  MergeNext(block).IgnoreError();

  // Try to split off a block of the requested size.
  Status status = Block::Split(block, aligned_inner_size).status();

  // It is not an error if the split fails because the remainder is too small
  // for a block.
  if (status == Status::ResourceExhausted()) {
    status = OkStatus();
  }

  // Otherwise, restore the original block on failure.
  if (!status.ok()) {
    Split(block, old_inner_size).IgnoreError();
  }
  block->MarkUsed();
  return status;
}

template <typename UintType, size_t kMaxSize>
Result<Block<UintType, kMaxSize>*> Block<UintType, kMaxSize>::Split(
    Block*& block, size_t new_inner_size) {
  if (block->Used()) {
    return Status::FailedPrecondition();
  }
  size_t old_inner_size = block->InnerSize();
  size_t aligned_inner_size = AlignUp(new_inner_size, kAlignment);
  if (old_inner_size < new_inner_size || old_inner_size < aligned_inner_size) {
    return Status::OutOfRange();
  }
  if (old_inner_size - aligned_inner_size < kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  size_t prev_offset = GetOffset(block->prev_);
  size_t outer_size1 = aligned_inner_size + kBlockOverhead;
  bool is_last = block->Last();
  UintType flags = block->GetFlags();
  ByteSpan bytes = AsBytes(std::move(block));
  Block* block1 = AsBlock(prev_offset, bytes.subspan(0, outer_size1));
  Block* block2 = AsBlock(outer_size1, bytes.subspan(outer_size1));
  size_t outer_size2 = block2->OuterSize();
  if (is_last) {
    block2->MarkLast();
  } else {
    SetOffset(block2->Next()->prev_, outer_size2);
  }
  block1->SetFlags(flags);
  BaseBlock::Poison(block1, kHeaderSize, outer_size1);
  BaseBlock::Poison(block2, kHeaderSize, outer_size2);
  block = std::move(block1);
  return block2;
}

template <typename UintType, size_t kMaxSize>
Status Block<UintType, kMaxSize>::MergeNext(Block*& block) {
  if (block == nullptr || block->Last()) {
    return Status::OutOfRange();
  }
  Block* next = block->Next();
  if (block->Used() || next->Used()) {
    return Status::FailedPrecondition();
  }
  size_t prev_offset = GetOffset(block->prev_);
  bool is_last = next->Last();
  UintType flags = block->GetFlags();
  ByteSpan prev_bytes = AsBytes(std::move(block));
  ByteSpan next_bytes = AsBytes(std::move(next));
  size_t next_offset = prev_bytes.size() + next_bytes.size();
  std::byte* merged = ::new (prev_bytes.data()) std::byte[next_offset];
  block = AsBlock(prev_offset, ByteSpan(merged, next_offset));
  if (is_last) {
    block->MarkLast();
  } else {
    SetOffset(block->Next()->prev_, GetOffset(block->next_));
  }
  block->SetFlags(flags);
  return OkStatus();
}

template <typename UintType, size_t kMaxSize>
Block<UintType, kMaxSize>* Block<UintType, kMaxSize>::Next() const {
  size_t offset = GetOffset(next_);
  uintptr_t addr = Last() ? 0 : reinterpret_cast<uintptr_t>(this) + offset;
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

template <typename UintType, size_t kMaxSize>
Block<UintType, kMaxSize>* Block<UintType, kMaxSize>::Prev() const {
  size_t offset = GetOffset(prev_);
  uintptr_t addr =
      (offset == 0) ? 0 : reinterpret_cast<uintptr_t>(this) - offset;
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

template <typename UintType, size_t kMaxSize>
void Block<UintType, kMaxSize>::SetFlags(UintType flags_to_set,
                                         UintType flags_to_clear) {
  UintType hi_flags_to_set = flags_to_set >> kCustomFlagBitsPerField;
  hi_flags_to_set <<= kCustomFlagShift;
  UintType hi_flags_to_clear = (flags_to_clear >> kCustomFlagBitsPerField)
                               << kCustomFlagShift;
  UintType lo_flags_to_set =
      (flags_to_set & ((UintType(1) << kCustomFlagBitsPerField) - 1))
      << kCustomFlagShift;
  UintType lo_flags_to_clear =
      (flags_to_clear & ((UintType(1) << kCustomFlagBitsPerField) - 1))
      << kCustomFlagShift;
  prev_ = (prev_ & ~hi_flags_to_clear) | hi_flags_to_set;
  next_ = (next_ & ~lo_flags_to_clear) | lo_flags_to_set;
}

template <typename UintType, size_t kMaxSize>
UintType Block<UintType, kMaxSize>::GetFlags() {
  UintType hi_flags = (prev_ & kCustomFlagMask) >> kCustomFlagShift;
  UintType lo_flags = (next_ & kCustomFlagMask) >> kCustomFlagShift;
  return (hi_flags << kCustomFlagBitsPerField) | lo_flags;
}

// Private template method implementations.

template <typename UintType, size_t kMaxSize>
Block<UintType, kMaxSize>::Block(size_t prev_offset, size_t next_offset)
    : BaseBlock() {
  SetOffset(prev_, prev_offset);
  SetOffset(next_, next_offset);
}

template <typename UintType, size_t kMaxSize>
ByteSpan Block<UintType, kMaxSize>::AsBytes(Block*&& block) {
  size_t block_size = block->OuterSize();
  std::byte* bytes = ::new (std::move(block)) std::byte[block_size];
  return {bytes, block_size};
}

template <typename UintType, size_t kMaxSize>
Block<UintType, kMaxSize>* Block<UintType, kMaxSize>::AsBlock(
    size_t prev_offset, ByteSpan bytes) {
  return ::new (bytes.data()) Block(prev_offset, bytes.size());
}

template <typename UintType, size_t kMaxSize>
typename Block<UintType, kMaxSize>::BlockStatus
Block<UintType, kMaxSize>::CheckStatus() const {
  // Make sure the Block is aligned.
  if (reinterpret_cast<uintptr_t>(this) % kAlignment != 0) {
    return BlockStatus::kMisaligned;
  }

  // Test if the prev/next pointer for this Block matches.
  if (!Last() && (this >= Next() || this != Next()->Prev())) {
    return BlockStatus::kNextMismatched;
  }

  if (Prev() && (this <= Prev() || this != Prev()->Next())) {
    return BlockStatus::kPrevMismatched;
  }

  if (!CheckPoison(this, kHeaderSize, OuterSize())) {
    return BlockStatus::kPoisonCorrupted;
  }

  return BlockStatus::kValid;
}

template <typename UintType, size_t kMaxSize>
void Block<UintType, kMaxSize>::CrashIfInvalid() {
  uintptr_t addr = reinterpret_cast<uintptr_t>(this);
  switch (CheckStatus()) {
    case kValid:
      break;
    case kMisaligned:
      CrashMisaligned(addr);
      break;
    case kNextMismatched:
      CrashNextMismatched(addr, reinterpret_cast<uintptr_t>(Next()->Prev()));
      break;
    case kPrevMismatched:
      CrashPrevMismatched(addr, reinterpret_cast<uintptr_t>(Prev()->Next()));
      break;
    case kPoisonCorrupted:
      CrashPoisonCorrupted(addr);
      break;
  }
}

}  // namespace pw::allocator
