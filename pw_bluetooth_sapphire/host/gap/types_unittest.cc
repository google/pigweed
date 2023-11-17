// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/gap/types.h"

#include <gtest/gtest.h>

namespace bt::gap {

TEST(TypesTest, SecurityPropertiesMeetRequirements) {
  std::array<hci_spec::LinkKeyType, 5> kUnauthenticatedNoScKeyTypes = {
      hci_spec::LinkKeyType::kCombination,
      hci_spec::LinkKeyType::kLocalUnit,
      hci_spec::LinkKeyType::kRemoteUnit,
      hci_spec::LinkKeyType::kDebugCombination,
      hci_spec::LinkKeyType::kUnauthenticatedCombination192};
  for (size_t i = 0; i < kUnauthenticatedNoScKeyTypes.size(); i++) {
    SCOPED_TRACE(i);
    sm::SecurityProperties props(kUnauthenticatedNoScKeyTypes[i]);
    EXPECT_TRUE(SecurityPropertiesMeetRequirements(
        props,
        BrEdrSecurityRequirements{.authentication = false,
                                  .secure_connections = false}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props,
        BrEdrSecurityRequirements{.authentication = false,
                                  .secure_connections = true}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props,
        BrEdrSecurityRequirements{.authentication = true,
                                  .secure_connections = false}));
    EXPECT_FALSE(SecurityPropertiesMeetRequirements(
        props,
        BrEdrSecurityRequirements{.authentication = true,
                                  .secure_connections = true}));
  }

  sm::SecurityProperties props(
      hci_spec::LinkKeyType::kAuthenticatedCombination192);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = true}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = true}));

  props = sm::SecurityProperties(
      hci_spec::LinkKeyType::kUnauthenticatedCombination256);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = true}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = false}));
  EXPECT_FALSE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = true}));

  props = sm::SecurityProperties(
      hci_spec::LinkKeyType::kAuthenticatedCombination256);
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = false,
                                .secure_connections = true}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = false}));
  EXPECT_TRUE(SecurityPropertiesMeetRequirements(
      props,
      BrEdrSecurityRequirements{.authentication = true,
                                .secure_connections = true}));
}

}  // namespace bt::gap
