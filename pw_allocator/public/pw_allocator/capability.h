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

#include <bitset>
#include <cstdint>

namespace pw::allocator {

/// Hints about optional methods implemented or optional behaviors requested
/// by an allocator of a derived type.
///
/// Implementations can provide hints by passing capabilities to the base
/// class constructor. These capabilities can be constructed by combining
/// ``Capability``s using logical operations.
enum Capability : uint32_t {
  // clang-format off
  kImplementsGetLayout = 1 << 0,
  kImplementsQuery     = 1 << 1,
  // clang-format on
};

/// A collection of ``Capability``s.
///
/// Concrete allocators should declare a constant set of capabilities, and pass
/// it to the ``Allocator`` constructor.
///
/// @code{.cpp}
/// class MyConcreteAllocator : public Allocator {
///  public:
///   static constexpr Capabilities kCapabilities = kCapability1 | kCapability2;
///
///   MyConcreteAllocator() : Allocator(kCapabilities) {}
/// };
/// @endcode
///
/// Forwarding allocators should pass the underlying allocator's capabilities,
/// potentially with modifications:
///
/// @code{.cpp}
/// class MyForwardingAllocator : public Allocator {
///  public:
///   MyForwardingAllocator(Allocator& allocator)
///     : Allocator(allocator.capabilities() | kCapability3),
///       allocator_(allocator) {}
/// };
/// @endcode
class Capabilities {
 public:
  constexpr Capabilities() : bits_(0) {}
  constexpr Capabilities(uint32_t bits) : bits_(bits) {}

  bool has(Capability capability) const {
    std::bitset<2> bits(capability);
    return (bits_ & bits) == bits;
  }

  uint32_t bits() const { return bits_.to_ulong(); }

 private:
  const std::bitset<2> bits_;
};

inline Capabilities operator|(const Capabilities& lhs,
                              const Capabilities& rhs) {
  return Capabilities(lhs.bits() | rhs.bits());
}

inline Capabilities operator&(const Capabilities& lhs,
                              const Capabilities& rhs) {
  return Capabilities(lhs.bits() & rhs.bits());
}

inline Capabilities operator^(const Capabilities& lhs,
                              const Capabilities& rhs) {
  return Capabilities(lhs.bits() ^ rhs.bits());
}

}  // namespace pw::allocator
