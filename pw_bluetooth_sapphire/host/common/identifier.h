// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_IDENTIFIER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_IDENTIFIER_H_

#include <cstdint>
#include <functional>
#include <string>

#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace common {

template <typename T>
struct IdentifierTraits {
  // Returns a string representation of |value|.
  static std::string ToString(T value) { return std::to_string(value); }
};

// Specializations for integer types return a fixed-length string.
template <>
struct IdentifierTraits<uint64_t> {
  static std::string ToString(uint64_t value) {
    return fxl::StringPrintf("%.16lx", value);
  }
};

// Opaque identifier type for host library layers.
template <typename T>
class Identifier {
  static_assert(std::is_trivial_v<T>);
  static_assert(!std::is_pointer_v<std::decay<T>>);

 public:
  using Traits = IdentifierTraits<T>;

  constexpr explicit Identifier(const T& value) : value_(value) {}
  Identifier() = default;

  T value() const { return value_; }

  // Comparison.
  bool operator==(const Identifier& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const Identifier& other) const {
    return value_ != other.value_;
  }

  // Returns a string representation of this identifier. This function allocates
  // memory.
  std::string ToString() const { return Traits::ToString(value_); }

 private:
  T value_;
};

// Opaque identifier type for Bluetooth peers.
class PeerId : public Identifier<uint64_t> {
 public:
  constexpr explicit PeerId(uint64_t value) : Identifier<uint64_t>(value) {}
  constexpr PeerId() : PeerId(0u) {}

  bool IsValid() const { return value() != 0u; }
};

constexpr PeerId kInvalidPeerId(0u);

// Generates a valid random peer identifier. This function can never return
// kInvalidPeerId.
PeerId RandomPeerId();

}  // namespace common
}  // namespace bt

// Specialization of std::hash for std::unordered_set, std::unordered_map, etc.
namespace std {

template <typename T>
struct hash<bt::common::Identifier<T>> {
  size_t operator()(const bt::common::Identifier<T>& id) const {
    return std::hash<T>()(id.value());
  }
};

template <>
struct hash<bt::common::PeerId> {
  size_t operator()(const bt::common::PeerId& id) const {
    return std::hash<decltype(id.value())>()(id.value());
  }
};

}  // namespace std

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_IDENTIFIER_H_
