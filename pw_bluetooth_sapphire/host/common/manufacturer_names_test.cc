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

TEST(ManufacturerNamesTest, NameIsHexString) {
  EXPECT_EQ("0x0000", GetManufacturerName(0x0000));
  EXPECT_EQ("0x1234", GetManufacturerName(0x1234));
  EXPECT_EQ("0x9999", GetManufacturerName(0x9999));
  EXPECT_EQ("0x0123", GetManufacturerName(0x0123));
  EXPECT_EQ("0x0fff", GetManufacturerName(0x0fff));
  EXPECT_EQ("0x0023", GetManufacturerName(0x0023));
  EXPECT_EQ("0xffff", GetManufacturerName(0xffff));
  EXPECT_EQ("0x0abc", GetManufacturerName(0x0abc));
}

}  // namespace
}  // namespace bt
