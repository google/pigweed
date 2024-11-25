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

#include "pw_bluetooth_proxy/internal/rfcomm_fcs.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

TEST(RfcommFcsTest, Uih) {
  std::array<uint8_t, 2> uih{
      0x19,
      0xEF,
  };
  auto view = emboss::MakeRfcommFrameView(uih.data(), uih.size());
  EXPECT_EQ(RfcommFcs(view), 0x55);
}

TEST(RfcommFcsTest, Sabm) {
  std::array<uint8_t, 3> sabm{
      0x19,
      0x2F,
      0x01,
  };
  auto view = emboss::MakeRfcommFrameView(sabm.data(), sabm.size());
  EXPECT_EQ(RfcommFcs(view), 0xA7);
}

TEST(RfcommFcsTest, SabmExtended) {
  std::array<uint8_t, 4> sabm_extended_length{
      0x19,
      0x2F,
      0x00,
      0x01,
  };
  auto view = emboss::MakeRfcommFrameView(sabm_extended_length.data(),
                                          sabm_extended_length.size());
  EXPECT_EQ(RfcommFcs(view), 0x61);
}

}  // namespace

}  // namespace pw::bluetooth::proxy
