// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth/hci_util.h"

#include <cstdint>

#include "pw_status/status.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth {

namespace {

TEST(HciUtilTest, GetHciHeaderSize_Command) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::COMMAND);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(3));
}

TEST(HciUtilTest, GetHciHeaderSize_Acl) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::ACL_DATA);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(4));
}

TEST(HciUtilTest, GetHciHeaderSize_Sync) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::SYNC_DATA);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(3));
}

TEST(HciUtilTest, GetHciHeaderSize_Event) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::EVENT);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(2));
}

TEST(HciUtilTest, GetHciHeaderSize_Iso) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::ISO_DATA);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(4));
}

TEST(HciUtilTest, GetHciHeaderSize_Unknown) {
  pw::Result<size_t> result = GetHciHeaderSize(emboss::H4PacketType::UNKNOWN);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::InvalidArgument());
}

TEST(HciUtilTest, GetHciHeaderSize_Invalid) {
  pw::Result<size_t> result =
      GetHciHeaderSize(static_cast<emboss::H4PacketType>(22));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::InvalidArgument());
}

TEST(HciUtilTest, GetHciPayloadSize_Command) {
  std::array<uint8_t, 3> data = {0x03, 0x02, 0x10};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::COMMAND, data);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(0x10));
}

TEST(HciUtilTest, GetHciPayloadSize_Command_OutOfRange) {
  std::array<uint8_t, 2> data = {0x03, 0x02};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::COMMAND, data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(HciUtilTest, GetHciPayloadSize_Acl) {
  std::array<uint8_t, 4> data = {0x0c, 0x00, 0x34, 0x12};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::ACL_DATA, data);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(0x1234));
}

TEST(HciUtilTest, GetHciPayloadSize_AclWithHci_OutOfRange) {
  std::array<uint8_t, 3> data = {0x0c, 0x00, 0x34};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::ACL_DATA, data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(HciUtilTest, GetHciPayloadSize_Sync) {
  std::array<uint8_t, 3> data = {0x02, 0x00, 0x06};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::SYNC_DATA, data);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(0x06));
}

TEST(HciUtilTest, GetHciPayloadSize_Sync_OutOfRange) {
  std::array<uint8_t, 2> data = {0x02, 0x00};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::SYNC_DATA, data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(HciUtilTest, GetHciPayloadSize_Event) {
  std::array<uint8_t, 2> data = {0x0e, 0x04};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::EVENT, data);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), static_cast<size_t>(0x04));
}

TEST(HciUtilTest, GetHciPayloadSize_Event_OutOfRange) {
  std::array<uint8_t, 1> data = {0x0e};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::EVENT, data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(HciUtilTest, GetHciPayloadSize_Iso) {
  std::array<uint8_t, 4> data = {0x02, 0x00, 0x12, 0xAB};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::ISO_DATA, data);
  EXPECT_TRUE(result.ok());
  // The size is 14 bits, extract it via bitmask
  EXPECT_EQ(result.value(), static_cast<size_t>(0xAB12 & 0x3FFF));
}

TEST(HciUtilTest, GetHciPayloadSize_Iso_OutOfRange) {
  std::array<uint8_t, 3> data = {0x02, 0x00, 0x06};
  pw::Result<size_t> result =
      GetHciPayloadSize(emboss::H4PacketType::ISO_DATA, data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(HciUtilTest, GetHciPayloadSize_Invalid) {
  std::array<uint8_t, 3> data = {0x03, 0x02, 0x10};
  pw::Result<size_t> result =
      GetHciPayloadSize(static_cast<emboss::H4PacketType>(22), data);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::InvalidArgument());
}

}  // namespace
}  // namespace pw::bluetooth
