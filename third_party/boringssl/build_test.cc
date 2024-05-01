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

#include <openssl/aes.h>

// Including just to make sure we don't get build failures due to -Werror
#include <openssl/base.h>
#include <openssl/bn.h>
#include <openssl/cmac.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/nid.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "pw_bytes/endian.h"
#include "pw_unit_test/framework.h"

namespace {

// Reusing code from the `pw_bluetooth_sapphire` project to exercise
// `boringssl` integration.
constexpr uint32_t k24BitMax = 0xFFFFFF;
constexpr size_t kUInt128Size = 16;
using UInt128 = std::array<uint8_t, kUInt128Size>;

void Swap128(const UInt128& in, UInt128* out) {
  for (size_t i = 0; i < in.size(); ++i) {
    (*out)[i] = in[in.size() - i - 1];
  }
}

TEST(Boringssl, UseAes) {
  // This just tests that building against boringssl, including headers, and
  // using functions works when depending on the `pw_third_party.boringssl`
  // module.
  const UInt128 irk{{0x9B,
                     0x7D,
                     0x39,
                     0x0A,
                     0xA6,
                     0x10,
                     0x10,
                     0x34,
                     0x05,
                     0xAD,
                     0xC8,
                     0x57,
                     0xA3,
                     0x34,
                     0x02,
                     0xEC}};
  const uint32_t prand = 0x708194;
  const uint32_t kExpected = 0x0DFBAA;

  // Ah()
  // r' = padding || r.
  UInt128 r_prime;
  r_prime.fill(0);
  *reinterpret_cast<uint32_t*>(r_prime.data()) =
      pw::bytes::ConvertOrderTo(pw::endian::little, prand & k24BitMax);

  // Encrypt()
  UInt128 hash128;
  const UInt128& key = irk;
  const UInt128& plaintext_data = r_prime;
  UInt128* out_encrypted_data = &hash128;

  UInt128 be_k, be_pt, be_enc;
  Swap128(key, &be_k);
  Swap128(plaintext_data, &be_pt);

  AES_KEY k;
  AES_set_encrypt_key(be_k.data(), 128, &k);
  AES_encrypt(be_pt.data(), be_enc.data(), &k);

  Swap128(be_enc, out_encrypted_data);

  auto ah =
      pw::bytes::ConvertOrderFrom(
          pw::endian::little, *reinterpret_cast<uint32_t*>(hash128.data())) &
      k24BitMax;
  EXPECT_EQ(kExpected, ah);
}
}  // namespace
