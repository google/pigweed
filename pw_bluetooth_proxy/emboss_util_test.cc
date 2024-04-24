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

#include "emboss_util.h"

#include "pw_bluetooth/hci_test.emb.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {
namespace {

TEST(EmbossUtilTest, CreateH4Subspan) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto span = H4HciSubspan(buffer);
  EXPECT_EQ(span.front(), buffer[1]);
  EXPECT_EQ(span.back(), buffer[3]);
  EXPECT_EQ(span.size(), buffer.size() - 1);
}

TEST(EmbossUtilTest, MakeViewFromSpan) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto span = pw::span(buffer);
  auto view = MakeEmboss<emboss::TestCommandPacketView>(span);
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x03);
}

TEST(EmbossUtilTest, MakeWriterFromSpan) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto span = pw::span(buffer);
  auto view = MakeEmboss<emboss::TestCommandPacketWriter>(span);
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x03);
}

TEST(EmbossUtilTest, MakeViewFromSubspan) {
  std::array<uint8_t, 5> buffer = {0x00, 0x01, 0x02, 0x03, 0x04};
  auto view = MakeEmboss<emboss::TestCommandPacketView>(H4HciSubspan(buffer));
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x04);
}

TEST(EmbossUtilTest, MakeWriterFromSubspan) {
  std::array<uint8_t, 5> buffer = {0x00, 0x01, 0x02, 0x03, 0x04};
  auto view = MakeEmboss<emboss::TestCommandPacketWriter>(H4HciSubspan(buffer));
  EXPECT_TRUE(view.IsComplete());
  EXPECT_EQ(view.payload().Read(), 0x04);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
