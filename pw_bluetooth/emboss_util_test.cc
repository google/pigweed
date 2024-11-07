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

#include "pw_bluetooth/emboss_util.h"

#include "pw_bluetooth/hci_test.emb.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {
namespace {

TEST(EmbossUtilTest, MakeEmbossViewFromSpan) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto span = pw::span(buffer);
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view, MakeEmbossView<emboss::TestCommandPacketView>(span));
  EXPECT_EQ(view.payload().Read(), 0x03);

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      view,
      MakeEmbossView<emboss::TestCommandPacketView>(span.data(), span.size()));
  EXPECT_EQ(view.payload().Read(), 0x03);

  auto failed_view =
      MakeEmbossView<emboss::TestCommandPacketView>(span.subspan(1));
  EXPECT_EQ(failed_view.status(), pw::Status::DataLoss());
}

TEST(EmbossUtilTest, MakeEmbossWriterFromSpan) {
  std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
  auto span = pw::span(buffer);
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view, MakeEmbossWriter<emboss::TestCommandPacketWriter>(span));
  EXPECT_EQ(view.payload().Read(), 0x03);

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      view,
      MakeEmbossWriter<emboss::TestCommandPacketWriter>(span.data(),
                                                        span.size()));
  EXPECT_EQ(view.payload().Read(), 0x03);

  auto failed_view =
      MakeEmbossWriter<emboss::TestCommandPacketWriter>(span.subspan(1));
  EXPECT_EQ(failed_view.status(), pw::Status::InvalidArgument());
}

}  // namespace
}  // namespace pw::bluetooth::proxy
