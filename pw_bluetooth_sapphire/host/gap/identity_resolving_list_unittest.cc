// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identity_resolving_list.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::gap {
namespace {

const DeviceAddress kAddress1(DeviceAddress::Type::kLERandom, {6, 5, 4, 3, 2, 1});
const DeviceAddress kAddress2(DeviceAddress::Type::kLERandom, {0x66, 0x55, 0x44, 0x33, 0x22, 0x11});

TEST(GAP_IdentityResolvingListTest, ResolveEmpty) {
  IdentityResolvingList rl;
  EXPECT_EQ(std::nullopt, rl.Resolve(kAddress1));
}

TEST(GAP_IdentityResolvingListTest, Resolve) {
  IdentityResolvingList rl;

  // Populate the list with two resolvable identities.
  UInt128 irk1 = Random<UInt128>();
  UInt128 irk2 = Random<UInt128>();
  rl.Add(kAddress1, irk1);
  rl.Add(kAddress2, irk2);

  // Generate RPAs from the IRKs. The list should be able to resolve them.
  DeviceAddress rpa1 = sm::util::GenerateRpa(irk1);
  DeviceAddress rpa2 = sm::util::GenerateRpa(irk2);

  auto identity1 = rl.Resolve(rpa1);
  ASSERT_TRUE(identity1);
  EXPECT_EQ(kAddress1, *identity1);

  auto identity2 = rl.Resolve(rpa2);
  ASSERT_TRUE(identity2);
  EXPECT_EQ(kAddress2, *identity2);

  // A resolvable address that can't be resolved by the list should report
  // failure.
  UInt128 unknown_irk = Random<UInt128>();
  DeviceAddress unknown_rpa = sm::util::GenerateRpa(unknown_irk);
  auto result = rl.Resolve(unknown_rpa);
  EXPECT_FALSE(result);

  // Removed identities should no longer resolve.
  rl.Remove(kAddress2);
  EXPECT_FALSE(rl.Resolve(rpa2));
  EXPECT_EQ(kAddress1, *rl.Resolve(rpa1));

  // Removing unknown devices should not crash.
  rl.Remove(unknown_rpa);
  rl.Remove(kAddress2);
}

// Tests that an identity address can be assigned a new IRK.
TEST(GAP_IdentityResolvingListTest, OverwriteIrk) {
  IdentityResolvingList rl;
  UInt128 irk1 = Random<UInt128>();
  UInt128 irk2 = Random<UInt128>();
  DeviceAddress rpa1 = sm::util::GenerateRpa(irk1);
  DeviceAddress rpa2 = sm::util::GenerateRpa(irk2);

  rl.Add(kAddress1, irk1);
  EXPECT_TRUE(rl.Resolve(rpa1));
  EXPECT_FALSE(rl.Resolve(rpa2));

  rl.Add(kAddress1, irk2);
  EXPECT_FALSE(rl.Resolve(rpa1));
  EXPECT_TRUE(rl.Resolve(rpa2));
}

}  // namespace
}  // namespace bt::gap
