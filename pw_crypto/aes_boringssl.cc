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

#include "pw_crypto/aes_boringssl.h"

#include <openssl/aes.h>
#include <openssl/cipher.h>
#include <openssl/cmac.h>

#include "pw_assert/check.h"
#include "pw_crypto/aes.h"
#include "pw_status/status.h"

namespace {
// Number of bits in a byte. BoringSSL requires a key size to be specified in
// bits.
constexpr size_t kBits = 8;
}  // namespace

namespace pw::crypto::aes::backend {
namespace {
using CmacCtx = NativeCmacContext;
}  // namespace

Status DoInit(CmacCtx& ctx, ConstByteSpan key) {
  CMAC_CTX* raw = CMAC_CTX_new();
  PW_CHECK_NOTNULL(raw);
  ctx = CmacCtx(raw, CmacContextDeleter());

  const EVP_CIPHER* cipher = nullptr;
  if (key.size() == kKey128SizeBytes) {
    cipher = EVP_aes_128_cbc();
  } else if (key.size() == kKey256SizeBytes) {
    cipher = EVP_aes_256_cbc();
  } else {
    PW_CRASH("Unsupported key size for Cmac (%zu bit)", key.size() * kBits);
  }

  if (!CMAC_Init(raw, key.data(), key.size(), cipher, nullptr)) {
    return Status::Internal();
  }

  return OkStatus();
}

Status DoUpdate(CmacCtx& ctx, ConstByteSpan data) {
  auto data_in = reinterpret_cast<const uint8_t*>(data.data());
  if (!CMAC_Update(ctx.get(), data_in, data.size())) {
    return Status::Internal();
  }

  return OkStatus();
}

Status DoFinal(CmacCtx& ctx, BlockSpan out_mac) {
  size_t unused_out_len;
  auto data_out = reinterpret_cast<uint8_t*>(out_mac.data());
  if (!CMAC_Final(ctx.get(), data_out, &unused_out_len)) {
    return Status::Internal();
  }

  return OkStatus();
}

Status DoEncryptBlock(ConstByteSpan key,
                      ConstBlockSpan plaintext,
                      BlockSpan out_ciphertext) {
  AES_KEY bssl_key;

  auto key_data = reinterpret_cast<const uint8_t*>(key.data());
  auto plaintext_data = reinterpret_cast<const uint8_t*>(plaintext.data());
  auto ciphertext_data = reinterpret_cast<uint8_t*>(out_ciphertext.data());

  PW_CHECK(AES_set_encrypt_key(key_data, key.size() * kBits, &bssl_key) == 0);
  AES_encrypt(plaintext_data, ciphertext_data, &bssl_key);

  return OkStatus();
}
}  // namespace pw::crypto::aes::backend
