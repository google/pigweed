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
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/block/contiguous.h"
#include "pw_allocator/config.h"

namespace pw::allocator {
namespace internal {

// Trivial base class for trait support.
struct PoisonableBase {};

}  // namespace internal

/// Mix-in for blocks that can be poisoned.
///
/// A poisoned block's usable space contains pattern of data whose integrity can
/// be checked later for modification.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `ContiguousBlock` and
/// provide the following method:
///
/// - static consextpr size_t ReservedWhenFree()
///   - Indicates how many bytes should not be poisoned.
/// - bool IsPoisoned() const
///   - Returns whether the block is poisoned.
/// - void SetPoisoned(bool)
///   - Sets whether the block is poisoned.
template <typename Derived>
class PoisonableBlock : public internal::PoisonableBase {
 protected:
  constexpr explicit PoisonableBlock() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_contiguous_v<Derived>,
                  "Types derived from PoisonableBlock must also derive from "
                  "ContiguousBlock");
  }

 public:
  static constexpr size_t kPoisonOffset = Derived::ReservedWhenFree();

  /// Returns the value written to a block's usable space when poisoning.
  constexpr uintptr_t GetPoisonWord() const {
    return derived()->DoGetPoisonWord();
  }

  /// Returns whether this block has been poisoned.
  bool IsPoisoned() const;

  /// Poisons the block's usable space.
  ///
  /// This method does nothing if the block is not free. The decision to poison
  /// a block is delegated to the allocator to allow for more nuanced strategies
  /// than simply all or nothing. For example, an allocator may want to balance
  /// security and performance by only poisoning every n-th free block.
  void Poison();

 protected:
  constexpr uintptr_t DoGetPoisonWord() const {
    // Hex dump looks like "defaced code is bad".
    return static_cast<uintptr_t>(0xAD5bE10DDCCEFADEULL);
  }

  /// @copydoc ContiguousBlock::DoSplitFirst
  Derived* DoSplitFirst(size_t new_inner_size);

  /// @copydoc ContiguousBlock::DoSplitLast
  Derived* DoSplitLast(size_t new_inner_size);

  /// @copydoc ContiguousBlock::DoMergeNext
  void DoMergeNext();

  /// @copydoc BasicBlock::CheckInvariants
  bool DoCheckInvariants(bool strict) const;

  /// Clears the poisoned state if a block is not free.
  void SetFree(bool is_free);

 private:
  constexpr Derived* derived() { return static_cast<Derived*>(this); }
  constexpr const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }

  /// Returns a pointer that can be used as an iterator to the poisonable
  /// region.
  constexpr uintptr_t* PoisonableBegin() const {
    auto addr = cpp20::bit_cast<uintptr_t>(derived()->UsableSpaceUnchecked());
    addr = AlignUp(addr + kPoisonOffset, sizeof(uintptr_t*));
    return cpp20::bit_cast<uintptr_t*>(addr);
  }

  /// Returns a pointer that can be used as a past-the-end iterator to the
  /// poisonable region.
  constexpr uintptr_t* PoisonableEnd() const {
    auto addr = cpp20::bit_cast<uintptr_t>(derived()->UsableSpaceUnchecked());
    addr =
        AlignDown(addr + derived()->InnerSizeUnchecked(), sizeof(uintptr_t*));
    return cpp20::bit_cast<uintptr_t*>(addr);
  }
};

/// Trait type that allow interrogating a block as to whether it is poisonable.
template <typename BlockType>
struct is_poisonable : std::is_base_of<internal::PoisonableBase, BlockType> {};

/// Helper variable template for `is_poisonable<BlockType>::value`.
template <typename BlockType>
inline constexpr bool is_poisonable_v = is_poisonable<BlockType>::value;

namespace internal {

/// Functions to crash with an error message describing which block invariant
/// has been violated. These functions are implemented independent of any
/// template parameters to allow it to use `PW_CHECK`.
[[noreturn]] void CrashPoisonCorrupted(uintptr_t addr);
[[noreturn]] void CrashPoisonedWhileInUse(uintptr_t addr);

}  // namespace internal

// Template method implementations.

template <typename Derived>
bool PoisonableBlock<Derived>::IsPoisoned() const {
  if constexpr (Hardening::kIncludesDebugChecks) {
    derived()->CheckInvariants();
  }
  return derived()->IsPoisonedUnchecked();
}

template <typename Derived>
void PoisonableBlock<Derived>::Poison() {
  if constexpr (Hardening::kIncludesDebugChecks) {
    derived()->CheckInvariants();
  }
  auto* begin = PoisonableBegin();
  auto* end = PoisonableEnd();
  if (begin < end) {
    std::fill(begin, end, derived()->GetPoisonWord());
    derived()->SetPoisoned(true);
  }
  if constexpr (Hardening::kIncludesDebugChecks) {
    derived()->CheckInvariants();
  }
}

template <typename Derived>
Derived* PoisonableBlock<Derived>::DoSplitFirst(size_t new_inner_size) {
  bool should_poison = derived()->IsPoisoned();
  derived()->SetPoisoned(false);
  Derived* trailing =
      derived()->ContiguousBlock<Derived>::DoSplitFirst(new_inner_size);
  if (should_poison) {
    trailing->SetPoisoned(true);
  }
  return trailing;
}

template <typename Derived>
Derived* PoisonableBlock<Derived>::DoSplitLast(size_t new_inner_size) {
  bool should_poison = derived()->IsPoisoned();
  derived()->SetPoisoned(false);
  Derived* trailing =
      derived()->ContiguousBlock<Derived>::DoSplitLast(new_inner_size);
  if (should_poison) {
    derived()->SetPoisoned(true);
  }
  return trailing;
}

template <typename Derived>
void PoisonableBlock<Derived>::DoMergeNext() {
  // Repoisoning is handle by the `BlockAllocator::DoDeallocate`.
  derived()->SetPoisoned(false);
  derived()->ContiguousBlock<Derived>::DoMergeNext();
}

template <typename Derived>
bool PoisonableBlock<Derived>::DoCheckInvariants(bool strict) const {
  auto addr = cpp20::bit_cast<uintptr_t>(this);
  if (!derived()->IsPoisonedUnchecked()) {
    return true;
  }
  if (!derived()->IsFreeUnchecked()) {
    if (strict) {
      internal::CrashPoisonedWhileInUse(addr);
    }
    return false;
  }
  auto* begin = PoisonableBegin();
  auto* end = PoisonableEnd();
  if (begin >= end) {
    return true;
  }
  bool poison_intact = std::all_of(
      begin, end, [this](uintptr_t word) { return word == GetPoisonWord(); });
  if (!poison_intact) {
    if (strict) {
      internal::CrashPoisonCorrupted(addr);
    }
    return false;
  }
  return true;
}

template <typename Derived>
void PoisonableBlock<Derived>::SetFree(bool is_free) {
  if (!is_free) {
    derived()->SetPoisoned(false);
  }
}

}  // namespace pw::allocator
