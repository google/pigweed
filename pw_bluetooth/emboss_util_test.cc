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
#include "pw_unit_test/framework.h"

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

TEST(CopyToEmbossTest, CopyArrayToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  auto e_span = pw::span(e_array);
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          e_span));

  std::array<uint8_t, 4> t1_array = {33, 71, 24, 91};

  UncheckedCopyToEmbossStruct(e_view.payload(), t1_array);
  EXPECT_TRUE(std::equal(e_view.payload().BackingStorage().begin(),
                         e_view.payload().BackingStorage().end(),
                         t1_array.begin(),
                         t1_array.end()));
}

TEST(CopyToEmbossTest, CopySpanToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 4> t1_array = {33, 71, 24, 91};

  UncheckedCopyToEmbossStruct(e_view.payload(), pw::span(t1_array));
  EXPECT_TRUE(std::equal(e_view.payload().BackingStorage().begin(),
                         e_view.payload().BackingStorage().end(),
                         t1_array.begin(),
                         t1_array.end()));
}

TEST(CopyToEmbossTest, CopySmallerToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 2> t1_array = {33, 71};

  UncheckedCopyToEmbossStruct(e_view.payload(), t1_array);
  EXPECT_TRUE(
      std::equal(e_view.payload().BackingStorage().begin(),
                 e_view.payload().BackingStorage().begin() + t1_array.size(),
                 t1_array.begin(),
                 t1_array.end()));
}

TEST(CopyToEmbossTest, CopyEmptyToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 0> t1_array = {};

  UncheckedCopyToEmbossStruct(e_view.payload(), t1_array);
  // Emboss view's underlying array is unchanged.
  EXPECT_TRUE(std::equal(e_view.BackingStorage().begin(),
                         e_view.BackingStorage().end(),
                         e_array.begin(),
                         e_array.end()));
}

TEST(CopyToEmbossTest, TryToCopyEmptyToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 0> t1_array = {};

  EXPECT_TRUE(TryToCopyToEmbossStruct(e_view.payload(), t1_array));
  // Emboss view's underlying array is unchanged.
  EXPECT_TRUE(std::equal(e_view.BackingStorage().begin(),
                         e_view.BackingStorage().end(),
                         e_array.begin(),
                         e_array.end()));
}

TEST(CopyToEmbossTest, TryToCopyToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 4> t1_array = {33, 71, 24, 91};

  EXPECT_TRUE(TryToCopyToEmbossStruct(e_view.payload(), t1_array));
  EXPECT_TRUE(std::equal(e_view.payload().BackingStorage().begin(),
                         e_view.payload().BackingStorage().end(),
                         t1_array.begin(),
                         t1_array.end()));
}

TEST(CopyToEmbossTest, TryToCopyTooLargeToEmboss) {
  std::array<uint8_t, 7> e_array = {0x00, 0x01, 0x02, 0x03};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      emboss::TestCommandPacketWithArrayPayloadWriter e_view,
      MakeEmbossWriter<emboss::TestCommandPacketWithArrayPayloadWriter>(
          pw::span{e_array}));

  std::array<uint8_t, 5> t1_array = {33, 71, 24, 91, 99};

  EXPECT_FALSE(TryToCopyToEmbossStruct(e_view.payload(), t1_array));
  // Emboss view's underlying array is unchanged.
  EXPECT_TRUE(std::equal(e_view.BackingStorage().begin(),
                         e_view.BackingStorage().end(),
                         e_array.begin(),
                         e_array.end()));
}

TEST(CopyToEmbossTest, TryToCopyToIncompleteEmboss) {
  std::array<uint8_t, 6> e_array = {0x00, 0x01, 0x02, 0x03};
  emboss::TestCommandPacketWithArrayPayloadWriter e_view{e_array.data(),
                                                         e_array.size()};

  std::array<uint8_t, 5> t1_array = {33, 71, 24, 91, 99};

  EXPECT_FALSE(TryToCopyToEmbossStruct(e_view.payload(), t1_array));
  // Emboss view's underlying array is unchanged.
  EXPECT_TRUE(std::equal(e_view.BackingStorage().begin(),
                         e_view.BackingStorage().end(),
                         e_array.begin(),
                         e_array.end()));
}

}  // namespace
}  // namespace pw::bluetooth::proxy
