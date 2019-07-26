// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permissions.h"

#include "gtest/gtest.h"

namespace bt {
namespace att {
namespace {

const AccessRequirements kDisallowed;
const AccessRequirements kNoSecurityReq(false, false, false);
const AccessRequirements kEncryptionReq(true, false, false);
const AccessRequirements kAuthenticationReq(false, true, false);
const AccessRequirements kAuthorizationReq(false, false, true);

const sm::SecurityProperties kNoSecurity(sm::SecurityLevel::kNoSecurity, 16, false);
const sm::SecurityProperties kEncrypted(sm::SecurityLevel::kEncrypted, 16, false);
const sm::SecurityProperties kAuthenticated(sm::SecurityLevel::kAuthenticated, 16, false);

TEST(ATT_PermissionsTest, ReadNotPermittedWhenDisallowed) {
  EXPECT_EQ(ErrorCode::kReadNotPermitted, CheckReadPermissions(kDisallowed, kNoSecurity));
  EXPECT_EQ(ErrorCode::kReadNotPermitted, CheckReadPermissions(kDisallowed, kEncrypted));
  EXPECT_EQ(ErrorCode::kReadNotPermitted, CheckReadPermissions(kDisallowed, kAuthenticated));
}

TEST(ATT_PermissionsTest, WriteNotPermittedWhenDisallowed) {
  EXPECT_EQ(ErrorCode::kWriteNotPermitted, CheckWritePermissions(kDisallowed, kNoSecurity));
  EXPECT_EQ(ErrorCode::kWriteNotPermitted, CheckWritePermissions(kDisallowed, kEncrypted));
  EXPECT_EQ(ErrorCode::kWriteNotPermitted, CheckWritePermissions(kDisallowed, kAuthenticated));
}

TEST(ATT_PermissionsTest, LinkNotSecure) {
  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kNoSecurityReq, kNoSecurity));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kNoSecurityReq, kNoSecurity));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kEncryptionReq, kNoSecurity));
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kEncryptionReq, kNoSecurity));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthenticationReq, kNoSecurity));
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthenticationReq, kNoSecurity));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthorizationReq, kNoSecurity));
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthorizationReq, kNoSecurity));
}

TEST(ATT_PermissionsTest, LinkEncrypted) {
  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kNoSecurityReq, kEncrypted));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kNoSecurityReq, kEncrypted));

  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kEncryptionReq, kEncrypted));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kEncryptionReq, kEncrypted));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthenticationReq, kEncrypted));
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthenticationReq, kEncrypted));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthorizationReq, kEncrypted));
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthorizationReq, kEncrypted));
}

TEST(ATT_PermissionsTest, LinkAuthenticated) {
  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kNoSecurityReq, kAuthenticated));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kNoSecurityReq, kAuthenticated));

  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kEncryptionReq, kAuthenticated));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kEncryptionReq, kAuthenticated));

  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kAuthenticationReq, kAuthenticated));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kAuthenticationReq, kAuthenticated));

  EXPECT_EQ(ErrorCode::kNoError, CheckReadPermissions(kAuthorizationReq, kAuthenticated));
  EXPECT_EQ(ErrorCode::kNoError, CheckWritePermissions(kAuthorizationReq, kAuthenticated));
}

}  // namespace
}  // namespace att
}  // namespace bt
