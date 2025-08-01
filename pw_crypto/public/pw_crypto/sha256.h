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
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/stream.h"

/// Cryptography library
namespace pw::crypto {

namespace sha256 {

/// The size of a SHA256 digest in bytes.
inline constexpr uint32_t kDigestSizeBytes = 32;

/// A state machine of a hashing session.
enum class Sha256State {
  /// Initialized and accepting input (via `Update()`).
  kReady = 1,

  /// Finalized by `Final()`. Any additional requests to `Update()` or `Final()`
  /// will trigger a transition to `kError`.
  kFinalized = 2,

  /// In an unrecoverable error state.
  kError = 3,
};

namespace backend {

// Primitive operations to be implemented by backends.
Status DoInit(NativeSha256Context& ctx);
Status DoUpdate(NativeSha256Context& ctx, ConstByteSpan data);
Status DoFinal(NativeSha256Context& ctx, ByteSpan out_digest);

}  // namespace backend

/// Computes the SHA256 digest of potentially long, non-contiguous input
/// messages.
///
/// Usage:
///
/// @code{.cpp}
/// if (!Sha256().Update(message).Update(more_message).Final(out_digest).ok()) {
///     // Error handling.
/// }
/// @endcode
class Sha256 {
 public:
  Sha256() {
    if (!backend::DoInit(native_ctx_).ok()) {
      PW_LOG_DEBUG("backend::DoInit() failed");
      state_ = Sha256State::kError;
      return;
    }

    state_ = Sha256State::kReady;
  }

  /// Feeds `data` to the running hasher. The feeding can involve zero
  /// or more `Update()` calls and the order matters.
  Sha256& Update(ConstByteSpan data) {
    if (state_ != Sha256State::kReady) {
      PW_LOG_DEBUG("The backend is not ready/initialized");
      return *this;
    }

    if (!backend::DoUpdate(native_ctx_, data).ok()) {
      PW_LOG_DEBUG("backend::DoUpdate() failed");
      state_ = Sha256State::kError;
      return *this;
    }

    return *this;
  }

  /// Finishes the hashing session and outputs the final digest in the
  /// first `kDigestSizeBytes` of `out_digest`. `out_digest` must be at least
  /// `kDigestSizeBytes` long.
  ///
  /// `Final()` locks down the `Sha256` instance from any additional use.
  ///
  /// Any error, including those occurring inside the constructor or `Update()`
  /// will be reflected in the return value of `Final()`.
  Status Final(ByteSpan out_digest) {
    if (out_digest.size() < kDigestSizeBytes) {
      PW_LOG_DEBUG("Digest output buffer is too small");
      state_ = Sha256State::kError;
      return Status::InvalidArgument();
    }

    if (state_ != Sha256State::kReady) {
      PW_LOG_DEBUG("The backend is not ready/initialized");
      return Status::FailedPrecondition();
    }

    auto status = backend::DoFinal(native_ctx_, out_digest);
    if (!status.ok()) {
      PW_LOG_DEBUG("backend::DoFinal() failed");
      state_ = Sha256State::kError;
      return status;
    }

    state_ = Sha256State::kFinalized;
    return OkStatus();
  }

 private:
  // Common hasher state. Tracked by the front-end.
  Sha256State state_;
  // Backend-specific context.
  backend::NativeSha256Context native_ctx_;
};

/// Calculates the SHA256 digest of `message` and stores the result
/// in `out_digest`. `out_digest` must be at least `kDigestSizeBytes` long.
///
/// One-shot digest example:
///
/// @code{.cpp}
/// #include "pw_crypto/sha256.h"
///
/// std::byte digest[32];
/// if (!pw::crypto::sha256::Hash(message, digest).ok()) {
///     // Handle errors.
/// }
///
/// // The content can also come from a pw::stream::Reader.
/// if (!pw::crypto::sha256::Hash(reader, digest).ok()) {
///     // Handle errors.
/// }
/// @endcode
///
/// Long, potentially non-contiguous message example:
///
/// @code{.cpp}
/// #include "pw_crypto/sha256.h"
///
/// std::byte digest[32];
///
/// if (!pw::crypto::sha256::Sha256()
///     .Update(chunk1).Update(chunk2).Update(chunk...)
///     .Final().ok()) {
///     // Handle errors.
/// }
/// @endcode
inline Status Hash(ConstByteSpan message, ByteSpan out_digest) {
  return Sha256().Update(message).Final(out_digest);
}

inline Status Hash(stream::Reader& reader, ByteSpan out_digest) {
  if (out_digest.size() < kDigestSizeBytes) {
    return Status::InvalidArgument();
  }

  Sha256 sha256;
  while (true) {
    Result<ByteSpan> res = reader.Read(out_digest);
    if (res.status().IsOutOfRange()) {
      break;
    }

    PW_TRY(res.status());
    sha256.Update(res.value());
  }

  return sha256.Final(out_digest);
}

}  // namespace sha256

}  // namespace pw::crypto
