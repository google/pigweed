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
#include <openssl/base.h>
#include <openssl/bn.h>
#include <openssl/cmac.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>
#include <openssl/nid.h>

#include "pw_bloat/bloat_this_binary.h"

int main() {
  pw::bloat::BloatThisBinary();
  uint8_t out[32];
  EC_KEY* new_key;
  new_key = EC_KEY_new_by_curve_name(EC_curve_nist2nid("P-256"));
  EC_KEY_generate_key(new_key);
  BIGNUM x, y;
  BN_init(&x);
  BN_init(&y);
  uint8_t pub_key_x[32];
  BN_le2bn(pub_key_x, sizeof(pub_key_x), &x);
  EC_KEY_set_private_key(new_key, &x);
  EC_KEY_set_public_key_affine_coordinates(new_key, &x, &y);
  EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(new_key),
                                      EC_KEY_get0_public_key(new_key),
                                      /*x=*/nullptr,
                                      &y,
                                      /*ctx=*/nullptr);
  ECDH_compute_key(out,
                   32,
                   EC_KEY_get0_public_key(new_key),
                   new_key,
                   /*kdf=*/nullptr);
  BN_bn2le_padded(out, 32, &y);
  AES_KEY k;
  AES_set_encrypt_key(out, 128, &k);
  AES_encrypt(out, out, &k);
  AES_CMAC(out, pub_key_x, 32, out, 32);
  BN_free(&x);
  BN_free(&y);
  EC_KEY_free(new_key);
  return 0;
}
