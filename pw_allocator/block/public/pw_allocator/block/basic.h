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

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/config.h"
#include "pw_assert/assert.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {
namespace internal {

/// Helper type that copies const-qualifers between types.
template <typename From, typename To>
struct CopyConst {
  using type = std::conditional_t<std::is_const_v<From>,
                                  std::add_const_t<To>,
                                  std::remove_const_t<To>>;
};

template <typename From, typename To>
using copy_const_t = typename CopyConst<From, To>::type;

template <typename From, typename To>
using copy_const_ptr_t =
    std::add_pointer_t<typename CopyConst<std::remove_pointer_t<From>,
                                          std::remove_pointer_t<To>>::type>;

// Trivial base class for trait support.
struct BasicBase {};

}  // namespace internal

/// Base mix-in for block implementations.
///
/// This CRTP-style type can be combined with block mix-in types. Block mix-ins
/// are stateless and trivially constructible. Mix-ins require the derived class
/// to implement methods to access and modify state, such has how to return a
/// block's size or a pointer to its usable memory.
///
/// The mix-ins also follow the NVI pattern. This allows mix-ins to layer
/// behavior for a particular method. For example, the implementation of
/// `AllocFirst` in `AlignableBlock` handles allocation with larger than default
/// alignment requirements, and delegates to `AllocatableBlock` for other
/// requests. The derived type provided by the concrete block implementation can
/// implement the public method that delegates to the correct mix-in.
/// Overridable methods are named `Do...`.
///
/// These mix-ins guarantee a number of invariants hold at the beginning and end
/// of each regular public API call. Each mix-in can enforce invariants by
/// overriding `DoCheckInvariants`. The concrete block implementation should
/// override the same method by calling each of the relevant mix-in methods.
/// Calling a public API method within an implementation of `DoCheckInvariants`
/// will lead to infinite recursion. To avoid this, mix-ins provide and/or
/// require versions of methods named `...Unchecked` that skip checking
/// invariants. These should only be used within the context of
/// `DoCheckInvariants` or other `...Unchecked` methods.
///
/// This mix-in requires its derived type provide the following symbols:
///
/// - static constexpr size_t DefaultAlignment()
///   - Returns the alignment of the block type. Must be a power of two.
/// - static constexpr size_t BlockOverhead()
///   - Returns the size of the metadata at the start of a block, before its
///     usable space.
/// - static constexpr size_t MinInnerSize()
///   - Returns the minimum inner size of a block. Should be 1 unless the usable
///     space is used to track blocks when they are free.
/// - size_t OuterSizeUnchecked() const
///   - Returns the size of the block. Must be multiple of `kAlignment`.
template <typename Derived>
class BasicBlock : public internal::BasicBase {
 public:
  static constexpr size_t kAlignment = Derived::DefaultAlignment();
  static constexpr size_t kBlockOverhead =
      AlignUp(Derived::BlockOverhead(), kAlignment);
  static constexpr size_t kMinOuterSize =
      kBlockOverhead + AlignUp(Derived::MinInnerSize(), kAlignment);

  BasicBlock() = default;
  ~BasicBlock() = default;

  // No copy or move.
  BasicBlock(const BasicBlock& other) = delete;
  BasicBlock& operator=(const BasicBlock& other) = delete;

  /// @returns  A pointer to a `Block`, given a pointer to the start of the
  ///           usable space inside the block.
  ///
  /// This is the inverse of `UsableSpace()`.
  ///
  /// @warning  This method does not do any checking; passing a random
  ///           pointer will return a non-null pointer.
  static inline Derived* FromUsableSpace(void* usable_space) {
    return FromUsableSpaceImpl(usable_space);
  }
  static inline const Derived* FromUsableSpace(const void* usable_space) {
    return FromUsableSpaceImpl(usable_space);
  }

  /// @returns A pointer to the usable space inside this block.
  inline std::byte* UsableSpace();
  inline const std::byte* UsableSpace() const;
  constexpr std::byte* UsableSpaceUnchecked() {
    return UsableSpaceUncheckedImpl(this);
  }
  constexpr const std::byte* UsableSpaceUnchecked() const {
    return UsableSpaceUncheckedImpl(this);
  }

  /// @returns The outer size of a block from the corresponding inner size.
  static size_t OuterSizeFromInnerSize(size_t inner_size);

  /// @returns The inner size of a block from the corresponding outer size.
  static size_t InnerSizeFromOuterSize(size_t outer_size);

  /// @returns The total size of the block in bytes, including the header.
  inline size_t OuterSize() const;

  /// @returns The number of usable bytes inside the block.
  inline size_t InnerSize() const;
  size_t InnerSizeUnchecked() const;

  /// @return whether a block is valid.
  inline bool IsValid() const;

  /// Does nothing unless `PW_ALLOCATOR_STRICT_VALIDATION` is set in the module
  /// configuration. If it is, calls `CheckInvariants` with `crash_on_failure`
  /// set. The method is static to avoid any checks of the pointer when strict
  /// validation is disabled.
  inline void CheckInvariantsIfStrict() const;

 protected:
  /// Like `IsValid`, but crashes if invalid and `crash_on_failure` is set.
  inline bool CheckInvariants(bool crash_on_failure) const;

  /// Performs the BasicBlock invariant checks.
  bool DoCheckInvariants(bool crash_on_failure) const;

 private:
  constexpr const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }

  /// Static version of `FromUsableSpace` that preserves constness.
  template <typename Ptr>
  static internal::copy_const_ptr_t<Ptr, Derived*> FromUsableSpaceImpl(
      Ptr usable_space);

  /// Static version of `UsableSpace` that preserves constness.
  template <typename Ptr>
  static constexpr internal::copy_const_ptr_t<Ptr, std::byte*>
  UsableSpaceUncheckedImpl(Ptr block) {
    using BytePtr = internal::copy_const_ptr_t<Derived, std::byte*>;
    auto addr = cpp20::bit_cast<uintptr_t>(block);
    PW_ASSERT(!PW_ADD_OVERFLOW(addr, Derived::kBlockOverhead, &addr));
    return cpp20::bit_cast<BytePtr>(addr);
  }
};

/// Trait type that allows interrogating whether a type is a block.
///
template <typename T>
struct is_block : std::is_base_of<internal::BasicBase, T> {};

/// Helper variable template for `is_block<T>::value`.
template <typename T>
inline constexpr bool is_block_v = is_block<T>::value;

namespace internal {

/// Function to crash with an error message describing which block invariant
/// has been violated. This function is implemented independent of any template
/// parameters to allow it to use `PW_CHECK`.
void CrashMisaligned(uintptr_t addr);

}  // namespace internal

// Template method implementations.

template <typename Derived>
template <typename Ptr>
internal::copy_const_ptr_t<Ptr, Derived*>
BasicBlock<Derived>::FromUsableSpaceImpl(Ptr usable_space) {
  using BlockPtr = internal::copy_const_ptr_t<Ptr, Derived*>;
  auto addr = cpp20::bit_cast<uintptr_t>(usable_space);
  if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
    PW_ASSERT(!PW_SUB_OVERFLOW(addr, kBlockOverhead, &addr));
  } else {
    addr -= kBlockOverhead;
  }
  auto* block = std::launder(reinterpret_cast<BlockPtr>(addr));
  block->CheckInvariantsIfStrict();
  return block;
}

template <typename Derived>
std::byte* BasicBlock<Derived>::UsableSpace() {
  CheckInvariantsIfStrict();
  return UsableSpaceUnchecked();
}

template <typename Derived>
const std::byte* BasicBlock<Derived>::UsableSpace() const {
  CheckInvariantsIfStrict();
  return UsableSpaceUnchecked();
}

template <typename Derived>
size_t BasicBlock<Derived>::OuterSizeFromInnerSize(size_t inner_size) {
  if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
    size_t outer_size;
    PW_ASSERT(!PW_ADD_OVERFLOW(inner_size, kBlockOverhead, &outer_size));
    return outer_size;
  } else {
    return inner_size + kBlockOverhead;
  }
}

template <typename Derived>
size_t BasicBlock<Derived>::InnerSizeFromOuterSize(size_t outer_size) {
  if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
    size_t inner_size;
    PW_ASSERT(!PW_SUB_OVERFLOW(outer_size, kBlockOverhead, &inner_size));
    return inner_size;
  } else {
    return outer_size - kBlockOverhead;
  }
}

template <typename Derived>
size_t BasicBlock<Derived>::OuterSize() const {
  CheckInvariantsIfStrict();
  return derived()->OuterSizeUnchecked();
}

template <typename Derived>
size_t BasicBlock<Derived>::InnerSize() const {
  CheckInvariantsIfStrict();
  return InnerSizeUnchecked();
}

template <typename Derived>
size_t BasicBlock<Derived>::InnerSizeUnchecked() const {
  return InnerSizeFromOuterSize(derived()->OuterSizeUnchecked());
}

template <typename Derived>
bool BasicBlock<Derived>::IsValid() const {
  return CheckInvariants(/*crash_on_failure=*/false);
}

template <typename Derived>
void BasicBlock<Derived>::CheckInvariantsIfStrict() const {
  if constexpr (PW_ALLOCATOR_STRICT_VALIDATION) {
    CheckInvariants(/* crash_on_failure: */ true);
  }
}

template <typename Derived>
bool BasicBlock<Derived>::CheckInvariants(bool crash_on_failure) const {
  return derived()->DoCheckInvariants(crash_on_failure);
}

template <typename Derived>
bool BasicBlock<Derived>::DoCheckInvariants(bool crash_on_failure) const {
  auto addr = cpp20::bit_cast<uintptr_t>(this);
  if (addr % Derived::kAlignment != 0) {
    if (crash_on_failure) {
      internal::CrashMisaligned(addr);
    }
    return false;
  }
  return true;
}

}  // namespace pw::allocator
