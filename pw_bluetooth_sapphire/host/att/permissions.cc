// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/att/permissions.h"

namespace bt::att {
namespace {

fit::result<ErrorCode> CheckSecurity(const AccessRequirements& reqs,
                                     const sm::SecurityProperties& security) {
  if (reqs.encryption_required() &&
      security.level() < sm::SecurityLevel::kEncrypted) {
    // If the peer is bonded but the link is not encrypted the "Insufficient
    // Encryption" error should be sent. Our GAP layer always keeps the link
    // encrypted so the authentication procedure needs to fail during
    // connection. We don't distinguish this from the un-paired state.
    // (NOTE: It is possible for the link to be authenticated without encryption
    // in LE Security Mode 2, which we do not support).
    return fit::error(ErrorCode::kInsufficientAuthentication);
  }

  if ((reqs.authentication_required() || reqs.authorization_required()) &&
      security.level() < sm::SecurityLevel::kAuthenticated) {
    return fit::error(ErrorCode::kInsufficientAuthentication);
  }

  if (reqs.encryption_required() &&
      security.enc_key_size() < reqs.min_enc_key_size()) {
    return fit::error(ErrorCode::kInsufficientEncryptionKeySize);
  }

  return fit::ok();
}

}  // namespace

fit::result<ErrorCode> CheckReadPermissions(
    const AccessRequirements& reqs, const sm::SecurityProperties& security) {
  if (!reqs.allowed()) {
    return fit::error(ErrorCode::kReadNotPermitted);
  }
  return CheckSecurity(reqs, security);
}

fit::result<ErrorCode> CheckWritePermissions(
    const AccessRequirements& reqs, const sm::SecurityProperties& security) {
  if (!reqs.allowed()) {
    return fit::error(ErrorCode::kWriteNotPermitted);
  }
  return CheckSecurity(reqs, security);
}

}  // namespace bt::att
