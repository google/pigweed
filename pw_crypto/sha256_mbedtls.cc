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
  // mbedtsl_sha256_init() never fails (returns void).
  mbedtls_sha256_init(&ctx_.native_context);

  ctx_.state = backend::Sha256State::kInitialized;
  if (mbedtls_sha256_starts_ret(&ctx_.native_context, /* is224 = */ 0)) {
    ctx_.state = backend::Sha256State::kError;
  }
}

void Sha256::Update(ConstByteSpan data) {
  if (ctx_.state != backend::Sha256State::kInitialized) {
    return;
  }

  if (mbedtls_sha256_update_ret(
          &ctx_.native_context,
          reinterpret_cast<const unsigned char*>(data.data()),
          data.size())) {
    ctx_.state = backend::Sha256State::kError;
    return;
  }

  return;
}

Status Sha256::Final(ByteSpan out_digest) {
  if (out_digest.size() < kDigestSizeBytes) {
    ctx_.state = backend::Sha256State::kError;
    return Status::InvalidArgument();
  }

  if (ctx_.state != backend::Sha256State::kInitialized) {
    return Status::FailedPrecondition();
  }

  if (mbedtls_sha256_finish_ret(
          &ctx_.native_context,
          reinterpret_cast<unsigned char*>(out_digest.data()))) {
    ctx_.state = backend::Sha256State::kError;
    return Status::Internal();
  }

  ctx_.state = backend::Sha256State::kFinalized;
  return OkStatus();
}

}  // namespace pw::crypto::sha256
