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

#include <mbedtls/cipher.h>

#include "pw_crypto/aes_backend_defs.h"

namespace pw::crypto::aes::backend {
constexpr auto kFullSupport =
    SupportedKeySize::k128 | SupportedKeySize::k192 | SupportedKeySize::k256;

/// The mbedtls backend supports 128-bit, 192-bit, and 256-bit keys for
/// unsafe EncryptBlock.
template <>
inline constexpr auto supported<AesOperation::kUnsafeEncryptBlock> =
    kFullSupport;
/// The mbedtls backend supports 128-bit, 192-bit, and 256-bit keys for CMAC.
template <>
inline constexpr auto supported<AesOperation::kCmac> = kFullSupport;

struct NativeCmacContext final {
  mbedtls_cipher_context_t cipher;

  NativeCmacContext() { mbedtls_cipher_init(&cipher); }
  ~NativeCmacContext() { mbedtls_cipher_free(&cipher); }

  NativeCmacContext(const NativeCmacContext&) = delete;
  NativeCmacContext& operator=(const NativeCmacContext& other) = delete;

  NativeCmacContext(NativeCmacContext&& other) : cipher(other.cipher) {
    other.cipher = {};
    // Necessary to init `other.cipher` even though `other` is an rvalue
    // because the deconstructor unconditionally calls `mbedtls_cipher_free`,
    // which expects a valid, initialized cipher.
    mbedtls_cipher_init(&other.cipher);
  }

  NativeCmacContext& operator=(NativeCmacContext&& other) {
    mbedtls_cipher_free(&cipher);
    cipher = other.cipher;
    other.cipher = {};
    // Necessary to init `other.cipher` even though `other` is an rvalue
    // because the deconstructor unconditionally calls `mbedtls_cipher_free`,
    // which expects a valid, initialized cipher.
    mbedtls_cipher_init(&other.cipher);
    return *this;
  }
};
}  // namespace pw::crypto::aes::backend
