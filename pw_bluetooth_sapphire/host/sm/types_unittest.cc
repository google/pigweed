// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include "gtest/gtest.h"

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
  EXPECT_EQ(SecurityLevel::kAuthenticated, props.level());
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

}  // namespace
}  // namespace sm
}  // namespace bt
