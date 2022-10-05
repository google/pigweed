// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permissions.h"

#include <gtest/gtest.h>

namespace bt::att {
namespace {

const AccessRequirements kDisallowed;
const AccessRequirements kNoSecurityReq(/*encryption=*/false, /*authentication=*/false,
                                        /*authorization=*/false);
const AccessRequirements kEncryptionReq(/*encryption=*/true, /*authentication=*/false,
                                        /*authorization=*/false);
const AccessRequirements kEncryptionWithMinKeySizeReq(/*encryption=*/true, /*authentication=*/false,
                                                      /*authorization=*/false, 7);
const AccessRequirements kAuthenticationReq(/*encryption=*/false, /*authentication=*/true,
                                            /*authorization=*/false);
const AccessRequirements kAuthorizationReq(/*encryption=*/false, /*authentication=*/false,
                                           /*authorization=*/true);
const AccessRequirements kAuthorizationWithMinKeySizeReq(/*encryption=*/false,
                                                         /*authentication=*/false,
                                                         /*authorization=*/true, 7);

const sm::SecurityProperties kNoSecurity(sm::SecurityLevel::kNoSecurity, 16,
                                         /*secure_connections=*/false);
const sm::SecurityProperties kEncrypted(sm::SecurityLevel::kEncrypted, 16,
                                        /*secure_connections=*/false);
const sm::SecurityProperties kEncryptedWithMinKeySize(sm::SecurityLevel::kEncrypted, 7,
                                                      /*secure_connections=*/false);
const sm::SecurityProperties kAuthenticated(sm::SecurityLevel::kAuthenticated, 16,
                                            /*secure_connections=*/false);
const sm::SecurityProperties kAuthenticatedWithMinKeySize(sm::SecurityLevel::kAuthenticated, 7,
                                                          /*secure_connections=*/false);

TEST(PermissionsTest, ReadNotPermittedWhenDisallowed) {
  EXPECT_EQ(ErrorCode::kReadNotPermitted,
            CheckReadPermissions(kDisallowed, kNoSecurity).error_value());
  EXPECT_EQ(ErrorCode::kReadNotPermitted,
            CheckReadPermissions(kDisallowed, kEncrypted).error_value());
  EXPECT_EQ(ErrorCode::kReadNotPermitted,
            CheckReadPermissions(kDisallowed, kAuthenticated).error_value());
}

TEST(PermissionsTest, WriteNotPermittedWhenDisallowed) {
  EXPECT_EQ(ErrorCode::kWriteNotPermitted,
            CheckWritePermissions(kDisallowed, kNoSecurity).error_value());
  EXPECT_EQ(ErrorCode::kWriteNotPermitted,
            CheckWritePermissions(kDisallowed, kEncrypted).error_value());
  EXPECT_EQ(ErrorCode::kWriteNotPermitted,
            CheckWritePermissions(kDisallowed, kAuthenticated).error_value());
}

TEST(PermissionsTest, LinkNotSecure) {
  EXPECT_EQ(fit::ok(), CheckReadPermissions(kNoSecurityReq, kNoSecurity));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kNoSecurityReq, kNoSecurity));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kEncryptionReq, kNoSecurity).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kEncryptionReq, kNoSecurity).error_value());

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthenticationReq, kNoSecurity).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthenticationReq, kNoSecurity).error_value());

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthorizationReq, kNoSecurity).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthorizationReq, kNoSecurity).error_value());
}

TEST(PermissionsTest, LinkEncrypted) {
  EXPECT_EQ(fit::ok(), CheckReadPermissions(kNoSecurityReq, kEncrypted));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kNoSecurityReq, kEncrypted));

  EXPECT_EQ(fit::ok(), CheckReadPermissions(kEncryptionReq, kEncrypted));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kEncryptionReq, kEncrypted));

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthenticationReq, kEncrypted).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthenticationReq, kEncrypted).error_value());

  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckReadPermissions(kAuthorizationReq, kEncrypted).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientAuthentication,
            CheckWritePermissions(kAuthorizationReq, kEncrypted).error_value());

  EXPECT_EQ(fit::ok(),
            CheckReadPermissions(kEncryptionWithMinKeySizeReq, kEncryptedWithMinKeySize));
  EXPECT_EQ(fit::ok(),
            CheckWritePermissions(kEncryptionWithMinKeySizeReq, kEncryptedWithMinKeySize));

  EXPECT_EQ(ErrorCode::kInsufficientEncryptionKeySize,
            CheckReadPermissions(kEncryptionReq, kEncryptedWithMinKeySize).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientEncryptionKeySize,
            CheckWritePermissions(kEncryptionReq, kEncryptedWithMinKeySize).error_value());
}

TEST(PermissionsTest, LinkAuthenticated) {
  EXPECT_EQ(fit::ok(), CheckReadPermissions(kNoSecurityReq, kAuthenticated));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kNoSecurityReq, kAuthenticated));

  EXPECT_EQ(fit::ok(), CheckReadPermissions(kEncryptionReq, kAuthenticated));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kEncryptionReq, kAuthenticated));

  EXPECT_EQ(fit::ok(), CheckReadPermissions(kAuthenticationReq, kAuthenticated));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kAuthenticationReq, kAuthenticated));

  EXPECT_EQ(fit::ok(), CheckReadPermissions(kAuthorizationReq, kAuthenticated));
  EXPECT_EQ(fit::ok(), CheckWritePermissions(kAuthorizationReq, kAuthenticated));

  EXPECT_EQ(ErrorCode::kInsufficientEncryptionKeySize,
            CheckReadPermissions(kEncryptionReq, kAuthenticatedWithMinKeySize).error_value());
  EXPECT_EQ(ErrorCode::kInsufficientEncryptionKeySize,
            CheckWritePermissions(kEncryptionReq, kAuthenticatedWithMinKeySize).error_value());
}

}  // namespace
}  // namespace bt::att
