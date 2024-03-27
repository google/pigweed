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

// All emboss headers are listed (even if they don't have explicit tests) to
// ensure they are compiled.
#include "pw_bluetooth/hci_commands.emb.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_h4.emb.h"      // IWYU pragma: keep
#include "pw_bluetooth/hci_test.emb.h"
#include "pw_bluetooth/hci_vendor.emb.h"    // IWYU pragma: keep
#include "pw_bluetooth/l2cap_frames.emb.h"  // IWYU pragma: keep
#include "pw_unit_test/framework.h"         // IWYU pragma: keep

namespace pw::bluetooth {
namespace {

TEST(EmbossTest, MakeView) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto view = emboss::MakeTestCommandPacketView(&buffer);
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x03);
}

// This definition has a mix of full-width values and bitfields and includes
// conditional bitfields. Let's add this to verify that the structure itself
// doesn't get changed incorrectly and that emboss' size calculation matches
// ours.
TEST(EmbossTest, CheckIsoHeaderSize) {
  EXPECT_EQ(emboss::IsoDataFrameHeader::MaxSizeInBytes(), 12);
}

}  // namespace
}  // namespace pw::bluetooth
