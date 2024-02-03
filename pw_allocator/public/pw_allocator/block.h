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
#include <cstdint>
#include <cstring>

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
/// allocated memory with a small amount of overhead. See
/// pw_allocator_private/simple_allocator.h for an example.
///
/// Blocks will always be aligned to a `kAlignment` boundary. Block sizes will
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
/// Each block may also include extra data and custom flags. The amount of extra
/// data is given in bytes by the `kNumExtraBytes` template parameter.
/// Additional bytes may be included in the header to keep it aligned to
/// `kAlignment`.
///
/// The custom flags are stored using bits from the offset fields, thereby
/// decreasing the range of offsets that blocks can address. Up to half of the
/// offset field may be used as flags, including one built-in flag per offset
/// field to track `used` and `last`.
///
/// @tparam   OffsetType      Unsigned integral type used to encode offsets and
///                           flags.
/// @tparam   kNumExtraBytes  Number of additional **bytes** to add to the block
///                           header storing custom data.
/// @tparam    kNumFlags      Number of **bits** of the offset fields to use as
///                           custom flags.
template <typename OffsetType = uintptr_t,
          size_t kNumExtraBytes = 0,
          size_t kNumFlags = 0>
class Block final : public BaseBlock {
 public:
  using offset_type = OffsetType;

  static_assert(std::is_unsigned_v<offset_type>);
  static_assert(kNumFlags < sizeof(offset_type) * CHAR_BIT);

  static constexpr size_t kAlignment = alignof(Block);
  static constexpr size_t kHeaderSize =
      AlignUp(sizeof(Block) + kNumExtraBytes + kPoisonOffset, kAlignment);
  static constexpr size_t kFooterSize = AlignUp(kPoisonOffset, kAlignment);
  static constexpr size_t kBlockOverhead = kHeaderSize + kFooterSize;

  /// @brief Creates the first block for a given memory region.
  ///
  /// @retval OK                    Returns a block representing the region.
  /// @retval INVALID_ARGUMENT      The region is null.
  /// @retval RESOURCE_EXHAUSTED    The region is too small for a block.
  /// @retval OUT_OF_RANGE          The region is too big to be addressed using
  ///                               `offset_type`.
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
  /// Custom flags are not copied when a block is split. When merging, the
  /// custom flags are preserved in the block that remains valid after the
  /// merge.
  ///
  /// If `flags_to_clear` are provided, these bits will be cleared before
  /// setting the `flags_to_set`. As a consequence, if a bit is set in both
  /// `flags_to_set` and `flags_to_clear`, it will be set upon return.
  ///
  /// @param[in]  flags_to_set      Bit flags to enable.
  /// @param[in]  flags_to_clear    Bit flags to disable.
  void SetFlags(offset_type flags_to_set, offset_type flags_to_clear = 0);

  /// Returns the custom flags previously set on this block.
  offset_type GetFlags();

  /// Stores extra data in the block.
  ///
  /// If the given region is shorter than `kNumExtraBytes`, it will be padded
  /// with `\x00` bytes. If the given region is longer than `kNumExtraBytes`, it
  /// will be truncated.
  ///
  /// Extra data is not copied when a block is split. When merging, the extra
  /// data is preserved in the block that remains valid after the merge.
  ///
  /// @param[in]  extra             Extra data to store in the block.
  void SetExtraBytes(ConstByteSpan extra);

  /// Stores extra data in the block from a trivially copyable type.
  ///
  /// The type given by template parameter should match the type used to specify
  /// `kNumExtraBytes`. The value will treated as a span of bytes and copied
  /// using `SetExtra(ConstByteSpan)`.
  template <typename T,
            std::enable_if_t<std::is_trivially_copyable_v<T> &&
                                 sizeof(T) == kNumExtraBytes,
                             int> = 0>
  void SetTypedExtra(const T& extra) {
    SetExtraBytes(as_bytes(span(&extra, 1)));
  }

  /// Returns the extra data from the block.
  ConstByteSpan GetExtraBytes() const;

  /// Returns the extra data from block as a default constructible and trivally
  /// copyable type.
  ///
  /// The template parameter should match the type used to specify
  /// `kNumExtraBytes`. For example:
  ///
  /// @code{.cpp}
  ///   using BlockType = Block<uint16_t, sizeof(Token)>;
  ///   BlockType* block = ...;
  ///   block->SetExtra(kMyToken);
  ///   Token my_token = block->GetExtra<Token>();
  /// @endcode
  template <typename T,
            std::enable_if_t<std::is_default_constructible_v<T> &&
                                 std::is_trivially_copyable_v<T> &&
                                 sizeof(T) == kNumExtraBytes,
                             int> = 0>
  T GetTypedExtra() const {
    T result{};
    std::memcpy(&result, GetExtraBytes().data(), kNumExtraBytes);
    return result;
  }

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
  static constexpr size_t kCustomFlagBitsPerField = (kNumFlags + 1) / 2;
  static constexpr size_t kOffsetBits =
      (sizeof(offset_type) * CHAR_BIT) - (kCustomFlagBitsPerField + 1);
  static constexpr offset_type kBuiltinFlag = offset_type(1) << kOffsetBits;
  static constexpr offset_type kOffsetMask = kBuiltinFlag - 1;
  static constexpr size_t kCustomFlagShift = kOffsetBits + 1;
  static constexpr offset_type kCustomFlagMask = ~(kOffsetMask | kBuiltinFlag);

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
  static size_t GetOffset(offset_type packed) {
    return static_cast<size_t>(packed & kOffsetMask) * kAlignment;
  }

  /// Overwrites the offset portion of `next_` or `prev_`.
  static void SetOffset(offset_type& field, size_t offset) {
    field =
        (field & ~kOffsetMask) | static_cast<offset_type>(offset) / kAlignment;
  }

  offset_type next_ = 0;
  offset_type prev_ = 0;

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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Result<Block<OffsetType, kNumExtraBytes, kNumFlags>*>
Block<OffsetType, kNumExtraBytes, kNumFlags>::Init(ByteSpan region) {
  if (region.data() == nullptr) {
    return Status::InvalidArgument();
  }
  auto addr = reinterpret_cast<uintptr_t>(region.data());
  auto aligned = AlignUp(addr, kAlignment);
  if (addr + region.size() <= aligned + kBlockOverhead) {
    return Status::ResourceExhausted();
  }
  region = region.subspan(aligned - addr);
  if (GetOffset(std::numeric_limits<offset_type>::max()) < region.size()) {
    return Status::OutOfRange();
  }
  Block* block = AsBlock(0, region);
  block->MarkLast();
  BaseBlock::Poison(block, kHeaderSize, block->OuterSize());
  return block;
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Status Block<OffsetType, kNumExtraBytes, kNumFlags>::AllocFirst(
    Block*& block, size_t inner_size, size_t alignment) {
  if (block->Used()) {
    return Status::FailedPrecondition();
  }
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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Status Block<OffsetType, kNumExtraBytes, kNumFlags>::AllocLast(
    Block*& block, size_t inner_size, size_t alignment) {
  if (block->Used()) {
    return Status::FailedPrecondition();
  }
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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
void Block<OffsetType, kNumExtraBytes, kNumFlags>::Free(Block*& block) {
  block->MarkFree();
  Block* prev = block->Prev();
  if (Block::MergeNext(prev).ok()) {
    block = prev;
  }
  Block::MergeNext(block).IgnoreError();
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Status Block<OffsetType, kNumExtraBytes, kNumFlags>::Resize(
    Block*& block, size_t new_inner_size) {
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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Result<Block<OffsetType, kNumExtraBytes, kNumFlags>*>
Block<OffsetType, kNumExtraBytes, kNumFlags>::Split(Block*& block,
                                                    size_t new_inner_size) {
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
  offset_type flags = block->GetFlags();
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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Status Block<OffsetType, kNumExtraBytes, kNumFlags>::MergeNext(Block*& block) {
  if (block == nullptr || block->Last()) {
    return Status::OutOfRange();
  }
  Block* next = block->Next();
  if (block->Used() || next->Used()) {
    return Status::FailedPrecondition();
  }
  size_t prev_offset = GetOffset(block->prev_);
  bool is_last = next->Last();
  offset_type flags = block->GetFlags();
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
  if constexpr (kNumFlags > 0) {
    block->SetFlags(flags);
  }
  return OkStatus();
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Block<OffsetType, kNumExtraBytes, kNumFlags>*
Block<OffsetType, kNumExtraBytes, kNumFlags>::Next() const {
  size_t offset = GetOffset(next_);
  uintptr_t addr = Last() ? 0 : reinterpret_cast<uintptr_t>(this) + offset;
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Block<OffsetType, kNumExtraBytes, kNumFlags>*
Block<OffsetType, kNumExtraBytes, kNumFlags>::Prev() const {
  size_t offset = GetOffset(prev_);
  uintptr_t addr =
      (offset == 0) ? 0 : reinterpret_cast<uintptr_t>(this) - offset;
  // See the note in `FromUsableSpace` about memory laundering.
  return std::launder(reinterpret_cast<Block*>(addr));
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
void Block<OffsetType, kNumExtraBytes, kNumFlags>::SetFlags(
    OffsetType flags_to_set, OffsetType flags_to_clear) {
  if constexpr (kNumFlags > 0) {
    offset_type hi_flags_to_set = flags_to_set >> kCustomFlagBitsPerField;
    hi_flags_to_set <<= kCustomFlagShift;
    offset_type hi_flags_to_clear = (flags_to_clear >> kCustomFlagBitsPerField)
                                    << kCustomFlagShift;
    offset_type lo_flags_to_set =
        (flags_to_set & ((offset_type(1) << kCustomFlagBitsPerField) - 1))
        << kCustomFlagShift;
    offset_type lo_flags_to_clear =
        (flags_to_clear & ((offset_type(1) << kCustomFlagBitsPerField) - 1))
        << kCustomFlagShift;
    prev_ = (prev_ & ~hi_flags_to_clear) | hi_flags_to_set;
    next_ = (next_ & ~lo_flags_to_clear) | lo_flags_to_set;
  }
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
OffsetType Block<OffsetType, kNumExtraBytes, kNumFlags>::GetFlags() {
  if constexpr (kNumFlags > 0) {
    offset_type hi_flags = (prev_ & kCustomFlagMask) >> kCustomFlagShift;
    offset_type lo_flags = (next_ & kCustomFlagMask) >> kCustomFlagShift;
    return (hi_flags << kCustomFlagBitsPerField) | lo_flags;
  }
  return 0;
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
void Block<OffsetType, kNumExtraBytes, kNumFlags>::SetExtraBytes(
    ConstByteSpan extra) {
  if constexpr (kNumExtraBytes > 0) {
    auto* data = reinterpret_cast<std::byte*>(this) + sizeof(*this);
    if (kNumExtraBytes <= extra.size()) {
      std::memcpy(data, extra.data(), kNumExtraBytes);
    } else {
      std::memcpy(data, extra.data(), extra.size());
      std::memset(data + extra.size(), 0, kNumExtraBytes - extra.size());
    }
  }
}

/// Returns the extra data from the block.
template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
ConstByteSpan Block<OffsetType, kNumExtraBytes, kNumFlags>::GetExtraBytes()
    const {
  if constexpr (kNumExtraBytes > 0) {
    const auto* data = reinterpret_cast<const std::byte*>(this) + sizeof(*this);
    return ConstByteSpan{data, kNumExtraBytes};
  } else {
    return ConstByteSpan{};
  }
}

// Private template method implementations.

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Block<OffsetType, kNumExtraBytes, kNumFlags>::Block(size_t prev_offset,
                                                    size_t next_offset)
    : BaseBlock() {
  SetOffset(prev_, prev_offset);
  SetOffset(next_, next_offset);
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
ByteSpan Block<OffsetType, kNumExtraBytes, kNumFlags>::AsBytes(Block*&& block) {
  size_t block_size = block->OuterSize();
  std::byte* bytes = ::new (std::move(block)) std::byte[block_size];
  return {bytes, block_size};
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
Block<OffsetType, kNumExtraBytes, kNumFlags>*
Block<OffsetType, kNumExtraBytes, kNumFlags>::AsBlock(size_t prev_offset,
                                                      ByteSpan bytes) {
  return ::new (bytes.data()) Block(prev_offset, bytes.size());
}

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
typename Block<OffsetType, kNumExtraBytes, kNumFlags>::BlockStatus
Block<OffsetType, kNumExtraBytes, kNumFlags>::CheckStatus() const {
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

template <typename OffsetType, size_t kNumExtraBytes, size_t kNumFlags>
void Block<OffsetType, kNumExtraBytes, kNumFlags>::CrashIfInvalid() {
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
