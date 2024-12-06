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
#include <type_traits>

namespace pw::crypto::aes::backend {
/// Possible supported AES operations. See `supports` for details.
enum class AesOperation : uint64_t {
  kUnsafeEncryptBlock,
};

/// Possible supported key sizes. See `supports` for details.
enum class SupportedKeySize : uint8_t {
  /// The operation is entirely unsupported for any key size.
  kUnsupported = 0,
  /// The operation supports 128-bit keys.
  k128 = 1 << 0,
  /// The operation supports 192-bit keys.
  k192 = 1 << 1,
  /// The operation supports 256-bit keys.
  k256 = 1 << 2,
};

/// Support bitwise & for `SupportedKeySize`.
constexpr SupportedKeySize operator&(SupportedKeySize x, SupportedKeySize y) {
  return static_cast<SupportedKeySize>(
      static_cast<std::underlying_type<SupportedKeySize>::type>(x) &
      static_cast<std::underlying_type<SupportedKeySize>::type>(y));
}

/// Support bitwise | for `SupportedKeySize`.
constexpr SupportedKeySize operator|(SupportedKeySize x, SupportedKeySize y) {
  return static_cast<SupportedKeySize>(
      static_cast<std::underlying_type<SupportedKeySize>::type>(x) |
      static_cast<std::underlying_type<SupportedKeySize>::type>(y));
}

/// Support bitwise ^ for `SupportedKeySize`.
constexpr SupportedKeySize operator^(SupportedKeySize x, SupportedKeySize y) {
  return static_cast<SupportedKeySize>(
      static_cast<std::underlying_type<SupportedKeySize>::type>(x) |
      static_cast<std::underlying_type<SupportedKeySize>::type>(y));
}

/// A backend must declare which operations it supports, and which key sizes it
/// supports for those operations. This declaration must be made in the
/// `pw::crypto::aes::backend` namespace in the header provided by the backend,
/// and it is always valid to override this for any `AesOperation`. For example,
/// to declare that the backend supports the operation
///`pw::crypto::aes::raw::EncryptBlock` with both 128-bit and 256-bit keys, do:
///
/// @code{.cpp}
/// namespace pw::crypto::aes::backend {
/// template<> inline constexpr SupportedKeySizes supported<kRawEncryptBlock> =
///   SupportedKeySize::k128 | SupportedKeySize::k256;
/// }
/// @endcode
///
/// By default all operations are unsupported for all key sizes, so a backend
/// must explicitly decleare that an operation is supported and which key sizes
/// it supports.
template <AesOperation>
inline constexpr SupportedKeySize supported = SupportedKeySize::kUnsupported;
}  // namespace pw::crypto::aes::backend
