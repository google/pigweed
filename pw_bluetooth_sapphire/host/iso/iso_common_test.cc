// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"

#include "pw_unit_test/framework.h"

namespace bt::iso {

TEST(IsoCommonTest, CigCisIdentifier) {
  CigCisIdentifier id1(0x10, 0x11);
  CigCisIdentifier id2(0x10, 0x12);
  CigCisIdentifier id3(0x11, 0x10);
  CigCisIdentifier id4(0x10, 0x10);
  CigCisIdentifier id5(0x10, 0x11);

  EXPECT_FALSE(id1 == id2);
  EXPECT_FALSE(id2 == id4);
  EXPECT_FALSE(id3 == id4);
  EXPECT_TRUE(id5 == id1);
  EXPECT_TRUE(id1 == id5);
  EXPECT_EQ(id1.cig_id(), 0x10);
  EXPECT_EQ(id1.cis_id(), 0x11);
}

}  // namespace bt::iso
