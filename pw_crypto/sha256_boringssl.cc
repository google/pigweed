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

#include "pw_crypto/sha256.h"
#include "pw_status/status.h"

namespace pw::crypto::sha256 {

Sha256::Sha256() {
  // SHA256_Init() always succeeds (returns 1).
  SHA256_Init(&ctx_.native_context);
  ctx_.finalized = false;
}

void Sha256::Update(ConstByteSpan data) {
  // SHA256_Update() always succeeds (returns 1).
  SHA256_Update(&ctx_.native_context,
                reinterpret_cast<const unsigned char*>(data.data()),
                data.size());
}

Status Sha256::Final(ByteSpan out_digest) {
  if (out_digest.size() < kDigestSizeBytes) {
    return Status::InvalidArgument();
  }

  if (ctx_.finalized) {
    // The behavior of calling Final after final is undefined and varies across
    // different implementations. In the BoringSSL case, it returns success but
    // produces a rotated digest.
    //
    // Here we check this condition explicitly and always return an error to
    // avoid any guess work.
    return Status::FailedPrecondition();
  }

  if (!SHA256_Final(reinterpret_cast<unsigned char*>(out_digest.data()),
                    &ctx_.native_context)) {
    return Status::Internal();
  }

  ctx_.finalized = true;

  return OkStatus();
}

}  // namespace pw::crypto::sha256
