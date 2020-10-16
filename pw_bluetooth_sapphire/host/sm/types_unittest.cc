// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include <string>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace sm {
namespace {

TEST(SMP_TypesTest, LinkKeyTypeToSecurityProperties) {
  SecurityProperties props(hci::LinkKeyType::kCombination);
  EXPECT_EQ(SecurityLevel::kNoSecurity, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kLocalUnit);
  EXPECT_EQ(SecurityLevel::kNoSecurity, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kRemoteUnit);
  EXPECT_EQ(SecurityLevel::kNoSecurity, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kDebugCombination);
  EXPECT_EQ(SecurityLevel::kEncrypted, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kUnauthenticatedCombination192);
  EXPECT_EQ(SecurityLevel::kEncrypted, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination192);
  EXPECT_EQ(SecurityLevel::kAuthenticated, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(true, props.authenticated());
  EXPECT_EQ(false, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kUnauthenticatedCombination256);
  EXPECT_EQ(SecurityLevel::kEncrypted, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(false, props.authenticated());
  EXPECT_EQ(true, props.secure_connections());

  props = SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination256);
  EXPECT_EQ(SecurityLevel::kSecureAuthenticated, props.level());
  EXPECT_EQ(16UL, props.enc_key_size());
  EXPECT_EQ(true, props.authenticated());
  EXPECT_EQ(true, props.secure_connections());
}

TEST(SMP_TypesTest, SecurityPropertiesToLinkKeyType) {
  SecurityProperties props(hci::LinkKeyType::kCombination);
  EXPECT_EQ(std::nullopt, props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kLocalUnit);
  EXPECT_EQ(std::nullopt, props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kRemoteUnit);
  EXPECT_EQ(std::nullopt, props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kDebugCombination);
  ASSERT_TRUE(props.GetLinkKeyType().has_value());
  EXPECT_EQ(hci::LinkKeyType::kUnauthenticatedCombination192, *props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kUnauthenticatedCombination192);
  ASSERT_TRUE(props.GetLinkKeyType().has_value());
  EXPECT_EQ(hci::LinkKeyType::kUnauthenticatedCombination192, *props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination192);
  ASSERT_TRUE(props.GetLinkKeyType().has_value());
  EXPECT_EQ(hci::LinkKeyType::kAuthenticatedCombination192, *props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kUnauthenticatedCombination256);
  ASSERT_TRUE(props.GetLinkKeyType().has_value());
  EXPECT_EQ(hci::LinkKeyType::kUnauthenticatedCombination256, *props.GetLinkKeyType());

  props = SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination256);
  ASSERT_TRUE(props.GetLinkKeyType().has_value());
  EXPECT_EQ(hci::LinkKeyType::kAuthenticatedCombination256, *props.GetLinkKeyType());
}

TEST(SMP_TypesTest, CorrectPropertiesToLevelMapping) {
  for (auto sc : {true, false}) {
    SCOPED_TRACE("secure connections: " + std::to_string(sc));
    for (auto key_sz : {kMinEncryptionKeySize, kMaxEncryptionKeySize}) {
      SCOPED_TRACE("encryption key size: " + std::to_string(key_sz));
      ASSERT_EQ(SecurityLevel::kEncrypted, SecurityProperties(true, false, sc, key_sz).level());

      for (auto auth : {true, false}) {
        SCOPED_TRACE("authenticated: " + std::to_string(auth));
        ASSERT_EQ(SecurityLevel::kNoSecurity, SecurityProperties(false, auth, sc, key_sz).level());
      }
    }
  }
  ASSERT_EQ(SecurityLevel::kAuthenticated,
            SecurityProperties(true, true, false, kMaxEncryptionKeySize).level());
  ASSERT_EQ(SecurityLevel::kAuthenticated,
            SecurityProperties(true, true, true, kMinEncryptionKeySize).level());
  ASSERT_EQ(SecurityLevel::kSecureAuthenticated,
            SecurityProperties(true, true, true, kMaxEncryptionKeySize).level());
}

TEST(SMP_TypesTest, PropertiesLevelConstructorWorks) {
  for (auto enc_key_size : {kMinEncryptionKeySize, kMaxEncryptionKeySize}) {
    SCOPED_TRACE("Enc key size: " + std::to_string(enc_key_size));
    for (auto sc : {true, false}) {
      SCOPED_TRACE("Secure Connections: " + std::to_string(sc));
      ASSERT_EQ(SecurityLevel::kNoSecurity,
                SecurityProperties(SecurityLevel::kNoSecurity, enc_key_size, sc).level());
      ASSERT_EQ(SecurityLevel::kEncrypted,
                SecurityProperties(SecurityLevel::kEncrypted, enc_key_size, sc).level());
      if (sc && enc_key_size == kMaxEncryptionKeySize) {
        ASSERT_EQ(SecurityLevel::kSecureAuthenticated,
                  SecurityProperties(SecurityLevel::kAuthenticated, enc_key_size, sc).level());
        ASSERT_EQ(
            SecurityLevel::kSecureAuthenticated,
            SecurityProperties(SecurityLevel::kSecureAuthenticated, enc_key_size, sc).level());
      } else {
        ASSERT_EQ(SecurityLevel::kAuthenticated,
                  SecurityProperties(SecurityLevel::kAuthenticated, enc_key_size, sc).level());
      }
    }
  }
}

TEST(SMP_TypesTest, HasKeysToDistribute) {
  PairingFeatures local_link_key_and_others;
  local_link_key_and_others.local_key_distribution = KeyDistGen::kLinkKey | KeyDistGen::kEncKey;
  EXPECT_TRUE(HasKeysToDistribute(local_link_key_and_others));

  PairingFeatures remote_link_key_and_others;
  remote_link_key_and_others.remote_key_distribution = KeyDistGen::kLinkKey | KeyDistGen::kIdKey;
  EXPECT_TRUE(HasKeysToDistribute(remote_link_key_and_others));

  PairingFeatures remote_link_key_only;
  remote_link_key_only.remote_key_distribution = KeyDistGen::kLinkKey;
  EXPECT_FALSE(HasKeysToDistribute(remote_link_key_only));

  // No keys set.
  EXPECT_FALSE(HasKeysToDistribute(PairingFeatures{}));
}
}  // namespace
}  // namespace sm
}  // namespace bt
