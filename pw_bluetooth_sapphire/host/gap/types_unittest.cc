// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include <gtest/gtest.h>

namespace bt::gap {

TEST(GAP_TypesTest, SecurityPropertiesMeetRequirements) {
  std::array<hci::LinkKeyType, 5> kUnauthenticatedNoScKeyTypes = {
      hci::LinkKeyType::kCombination, hci::LinkKeyType::kLocalUnit, hci::LinkKeyType::kRemoteUnit,
      hci::LinkKeyType::kDebugCombination, hci::LinkKeyType::kUnauthenticatedCombination192};
  for (size_t i = 0; i < kUnauthenticatedNoScKeyTypes.size(); i++) {
    SCOPED_TRACE(i);
    sm::SecurityProperties props(kUnauthenticatedNoScKeyTypes[i]);
    EXPECT_TRUE(SecurityPropertiesMeetRequirements(
        props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = true}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = false}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = true}));
  }

  sm::SecurityProperties props(hci::LinkKeyType::kAuthenticatedCombination192);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = true}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = true}));

  props = sm::SecurityProperties(hci::LinkKeyType::kUnauthenticatedCombination256);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = true}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = true}));

  props = sm::SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination256);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = false, .secure_connections = true}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props, BrEdrSecurityRequirements{.authentication = true, .secure_connections = true}));
}

}  // namespace bt::gap
