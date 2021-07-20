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

#pragma once

#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_crypto/sha256_backend.h"
#include "pw_status/status.h"

namespace pw::crypto::sha256 {

// Size in bytes of a SHA256 digest.
constexpr uint32_t kDigestSizeBytes = 32;

// Sha256 computes the SHA256 digest of potentially long, non-contiguous input
// messages.
class Sha256 {
 public:
  Sha256();
  // Remove copy/move ctors
  Sha256(const Sha256& other) = delete;
  Sha256(Sha256&& other) = delete;
  Sha256& operator=(const Sha256& other) = delete;
  Sha256& operator=(Sha256&& other) = delete;

  // Update adds `data` to the running hasher.
  void Update(ConstByteSpan data);

  // Final outputs the digest in `out_digest` and resets the hasher state.
  // `out_digest` must be at least `kDigestSizeBytes` long.
  Status Final(ByteSpan out_digest);

 private:
  backend::Sha256Context ctx_;
};

// Hash calculates the SHA256 digest of `message` and stores the result
// in `out_digest`. `out_digest` must be at least `kDigestSizeBytes` long.
inline Status Hash(ConstByteSpan message, ByteSpan out_digest) {
  Sha256 h;
  h.Update(message);
  return h.Final(out_digest);
}

}  // namespace pw::crypto::sha256
