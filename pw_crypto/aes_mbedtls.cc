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

#include "pw_crypto/aes_mbedtls.h"

#include <mbedtls/aes.h>

#include "pw_assert/check.h"
#include "pw_crypto/aes.h"
#include "pw_status/status.h"

namespace {
/// Number of bits in a byte.
constexpr size_t kBits = 8;
}  // namespace

namespace pw::crypto::aes::backend {

Status DoEncryptBlock(ConstByteSpan key,
                      ConstBlockSpan plaintext,
                      BlockSpan out_ciphertext) {
  const auto key_data = reinterpret_cast<const unsigned char*>(key.data());
  const auto in = reinterpret_cast<const unsigned char*>(plaintext.data());
  const auto out = reinterpret_cast<unsigned char*>(out_ciphertext.data());

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  if (mbedtls_aes_setkey_enc(&aes, key_data, key.size() * kBits)) {
    mbedtls_aes_free(&aes);
    return Status::Internal();
  }
  if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in, out)) {
    mbedtls_aes_free(&aes);
    return Status::Internal();
  }

  mbedtls_aes_free(&aes);
  return OkStatus();
}

}  // namespace pw::crypto::aes::backend
