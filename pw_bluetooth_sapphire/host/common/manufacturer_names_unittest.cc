// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/common/manufacturer_names.h"

#include <gtest/gtest.h>

namespace bt {
namespace {

TEST(ManufacturerNamesTest, ExhaustiveLookUp) {
  constexpr uint16_t kReservedId = 0x049E;

  // Looking up beyond the maximum entry shouldn't crash.
  EXPECT_EQ("", GetManufacturerName(kReservedId));

  // Do an incremental look up of all entries up to |kReservedId|. The
  // code shouldn't crash. We don't make an exact comparison of all entries
  // since this is a resiliency test.
  for (uint16_t i = 0; i < kReservedId; ++i) {
    std::string result = GetManufacturerName(i);
    EXPECT_FALSE(result.empty()) << "Failed for id: " << i;
  }
}

}  // namespace
}  // namespace bt
