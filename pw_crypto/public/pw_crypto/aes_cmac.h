// Copyright 2025 The Pigweed Authors
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

#include "pw_crypto/aes.h"

namespace pw::crypto::aes_cmac {

/// Computes the message authentication code (MAC) of a message using AES-CMAC.
///
/// The interface specifically allows computing the MAC of potentially long,
/// non-contiguous messages. A MAC is similar to a message digest in that it can
/// be used to verify integrity, but since it also takes a secret `key` as input
/// it can also be used to verify authenticity, as the other party must also
/// know the secret key to compute the same MAC.
///
/// Usage:
///
/// @code{.cpp}
/// if (!Cmac(key).Update(part1).Update(part2).Final(out_mac).ok()) {
///     // Error handling.
/// }
/// @endcode
class Cmac {
 private:
  enum class State {
    /// Initialized and accepting input (via `Update()`).
    kReady = 1,
    /// Finalized by `Final()`. Any additional requests to `Update()` or
    /// `Final()` will trigger a transition to `kError`.
    kFinalized = 2,
    /// In an unrecoverable error state.
    kError = 3,
  };

 public:
  /// Initialize a `Cmac` instance using the specified `key`.
  ///
  /// @note Any error during initialization will be reflected in the return
  /// value of `Final()`.
  ///
  /// @param[in] key A byte string containing the key to used in the CMAC
  /// operation.
  template <size_t key_size>
  explicit Cmac(span<const std::byte, key_size> key) {
    constexpr auto this_op = pw::crypto::aes::backend::AesOperation::kCmac;
    static_assert(pw::crypto::aes::internal::BackendSupports<this_op>(key_size),
                  "Usupported key size for CMAC for backend.");

    if (!pw::crypto::aes::backend::DoInit(native_ctx_, key).ok()) {
      PW_LOG_DEBUG("backend::DoInit() failed");
      state_ = State::kError;
      return;
    }

    state_ = State::kReady;
  }

  explicit Cmac(span<const std::byte, dynamic_extent> key) {
    constexpr auto this_op = pw::crypto::aes::backend::AesOperation::kCmac;
    PW_ASSERT(pw::crypto::aes::internal::BackendSupports<this_op>(key.size()));

    if (!pw::crypto::aes::backend::DoInit(native_ctx_, key).ok()) {
      PW_LOG_DEBUG("backend::DoInit() failed");
      state_ = State::kError;
      return;
    }

    state_ = State::kReady;
  }

  // Overload to enable implicit conversions to span if `T` is implicitly
  // convertible to `span`.
  template <typename T,
            typename = std::enable_if_t<
                std::is_convertible_v<T, decltype(span(std::declval<T>()))>>>
  explicit Cmac(const T& key) : Cmac(span(key)) {}

  /// Feeds `data` to the running AES-CMAC operation.
  ///
  /// The feeding can involve zero or more `Update()` calls and the order
  /// matters.
  ///
  /// @note Any error during update will be reflected in the return value of
  /// `Final()`.
  ///
  /// @param[in] data A byte string of any length to be fed to the running
  /// AES-CMAC operation.
  ///
  /// @returns This same `Cmac` instance to allow chaining calls.
  Cmac& Update(ConstByteSpan data) {
    if (data.empty()) {
      return *this;
    }

    if (state_ != State::kReady) {
      PW_LOG_DEBUG("The backend is not ready/initialized");
      return *this;
    }

    if (!pw::crypto::aes::backend::DoUpdate(native_ctx_, data).ok()) {
      PW_LOG_DEBUG("backend::DoUpdate() failed");
      state_ = State::kError;
      return *this;
    }

    return *this;
  }

  /// Finishes the AES-CMAC operations and outputs the final MAC.
  ///
  /// Additionally, `Final()` locks down the `Cmac` instance from any additional
  /// use.
  ///
  /// @note Any error during initialization or update will be reflected in the
  /// return value of `Final()`.
  ///
  /// @param[in] out_mac A byte span with a size of at least `kBlockSizeBytes`
  /// where the final MAC will be written. If the span is larger than
  /// `kBlockSizeBytes` only the first `kBlockSizeBytes` will be modified.
  ///
  /// @returns @pw_status{OK} if the AES-CMAC operation was successful and the
  /// MAC was written to `out_mac`, @pw_status{RESOURCE_EXHAUSTED} if `out_mac`
  /// is too small to write the MAC to, or @pw_status{INTERNAL} if an error was
  /// encountered during the operation.
  Status Final(aes::BlockSpan out_mac) {
    if (state_ != State::kReady) {
      PW_LOG_DEBUG("The backend is not ready/initialized");
      return Status::FailedPrecondition();
    }

    auto status = pw::crypto::aes::backend::DoFinal(native_ctx_, out_mac);
    if (!status.ok()) {
      PW_LOG_DEBUG("backend::DoFinal() failed");
      state_ = State::kError;
      return status;
    }

    state_ = State::kFinalized;
    return OkStatus();
  }

 private:
  // Common state. Tracked by the front-end.
  State state_;
  // Backend-specific context.
  pw::crypto::aes::backend::NativeCmacContext native_ctx_;
};

}  // namespace pw::crypto::aes_cmac
