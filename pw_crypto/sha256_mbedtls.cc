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
  // mbedtsl_md_init() never fails (returns void).
  mbedtls_md_init(&ctx_.native_context);
  ctx_.state = backend::Sha256State::kInitialized;
}

void Sha256::Update(ConstByteSpan data) {
  if (ctx_.state == backend::Sha256State::kInitialized) {
    if (mbedtls_md_setup(&ctx_.native_context,
                         mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                         /* hmac = */ 0)) {
      ctx_.state = backend::Sha256State::kError;
      return;
    }

    if (mbedtls_md_starts(&ctx_.native_context)) {
      ctx_.state = backend::Sha256State::kError;
      return;
    }

    ctx_.state = backend::Sha256State::kStarted;
  }

  if (ctx_.state != backend::Sha256State::kStarted) {
    return;
  }

  if (mbedtls_md_update(&ctx_.native_context,
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

  if (ctx_.state == backend::Sha256State::kInitialized) {
    // It is OK for users to forget or skip Update().
    Update({});
  }

  if (ctx_.state != backend::Sha256State::kStarted) {
    return Status::FailedPrecondition();
  }

  if (mbedtls_md_finish(&ctx_.native_context,
                        reinterpret_cast<unsigned char*>(out_digest.data()))) {
    ctx_.state = backend::Sha256State::kError;
    return Status::Internal();
  }

  ctx_.state = backend::Sha256State::kFinalized;
  return OkStatus();
}

}  // namespace pw::crypto::sha256
