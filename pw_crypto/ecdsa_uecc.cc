// Copyright 2021 The Pigweed Authors
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
#define PW_LOG_MODULE_NAME "ECDSA-UECC"
#define PW_LOG_LEVEL PW_LOG_LEVEL_WARN

#include <cstring>

#include "pw_crypto/ecdsa.h"
#include "pw_log/log.h"
#include "uECC.h"

namespace pw::crypto::ecdsa {

constexpr size_t kP256CurveOrderBytes = 32;
constexpr size_t kP256PublicKeySize = 2 * kP256CurveOrderBytes + 1;
constexpr size_t kP256SignatureSize = kP256CurveOrderBytes * 2;

Status VerifyP256Signature(ConstByteSpan public_key,
                           ConstByteSpan digest,
                           ConstByteSpan signature) {
  // Signature expected in raw format (r||s)
  if (signature.size() != kP256SignatureSize) {
    PW_LOG_DEBUG("Bad signature format");
    return Status::InvalidArgument();
  }

  // Supports SEC 1 uncompressed form (04||X||Y) only.
  if (public_key.size() != kP256PublicKeySize ||
      std::to_integer<uint8_t>(public_key.data()[0]) != 0x04) {
    PW_LOG_DEBUG("Bad public key format");
    return Status::InvalidArgument();
  }

#if uECC_VLI_NATIVE_LITTLE_ENDIAN
  // VerifyP256Signature always assume big endian input. If the library is
  // configured to expect native little endian, we need to convert the input
  // into little endian.
  uint8_t signature_bytes[kP256SignatureSize]
      __attribute__((__aligned__(sizeof(uint64_t))));
  memcpy(signature_bytes, signature.data(), sizeof(signature_bytes));
  std::reverse(signature_bytes, signature_bytes + kP256CurveOrderBytes);  // r
  std::reverse(signature_bytes + kP256CurveOrderBytes,
               signature_bytes + sizeof(signature_bytes));  // s

  uint8_t public_key_bytes[kP256PublicKeySize - 1]
      __attribute__((__aligned__(sizeof(uint64_t))));
  memcpy(public_key_bytes, public_key.data() + 1, sizeof(public_key_bytes));
  std::reverse(public_key_bytes, public_key_bytes + kP256CurveOrderBytes);  // X
  std::reverse(public_key_bytes + kP256CurveOrderBytes,
               public_key_bytes + sizeof(public_key_bytes));  // Y

  uint8_t digest_bytes[kP256CurveOrderBytes]
      __attribute__((__aligned__(sizeof(uint64_t))));
  memcpy(digest_bytes, digest.data(), sizeof(digest_bytes));
  std::reverse(digest_bytes, digest_bytes + sizeof(digest_bytes));
#else
  const uint8_t* public_key_bytes =
      reinterpret_cast<const uint8_t*>(public_key.data()) + 1;
  const uint8_t* digest_bytes = reinterpret_cast<const uint8_t*>(digest.data());
  const uint8_t* signature_bytes =
      reinterpret_cast<const uint8_t*>(signature.data());
#endif

  uECC_Curve curve = uECC_secp256r1();
  // Make sure the public key is on the curve.
  if (!uECC_valid_public_key(public_key_bytes, curve)) {
    PW_LOG_DEBUG("Bad public key curve");
    return Status::InvalidArgument();
  }

  // Digests must be at least 32 bytes. Digests longer than 32
  // bytes are truncated to 32 bytes.
  if (digest.size() < kP256CurveOrderBytes) {
    PW_LOG_DEBUG("Digest is too short");
    return Status::InvalidArgument();
  }

  // Verify the signature.
  if (!uECC_verify(public_key_bytes,
                   digest_bytes,
                   digest.size(),
                   signature_bytes,
                   curve)) {
    PW_LOG_DEBUG("Signature verification failed");
    return Status::Unauthenticated();
  }

  return OkStatus();
}

}  // namespace pw::crypto::ecdsa
