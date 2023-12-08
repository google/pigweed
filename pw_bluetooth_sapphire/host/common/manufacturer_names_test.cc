// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/common/manufacturer_names.h"

#include "pw_unit_test/framework.h"

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
