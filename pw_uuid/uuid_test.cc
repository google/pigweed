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

#include "pw_uuid/uuid.h"

#include <array>
#include <cstdint>

#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::uuid {
namespace {

TEST(Uuid, UuidToString_FromSpan_uint8) {
  constexpr std::array<uint8_t, 16> data = {
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x08,
      0x11,
      0x12,
      0x13,
      0x14,
      0x15,
      0x16,
      0x17,
      0x18,
  };
  constexpr Result<Uuid> uuid = Uuid::FromSpan(data);
  EXPECT_EQ(OkStatus(), uuid.status());
  EXPECT_EQ("18171615-1413-1211-0807-060504030201", uuid.value().ToString());
}

TEST(Uuid, UuidToString_FromSpan_uint8_bad) {
  std::array<uint8_t, 15> data_too_small = {};
  constexpr Result<Uuid> uuid_too_small = Uuid::FromSpan(data_too_small);
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_small.status());

  std::array<uint8_t, 17> data_too_large = {};
  constexpr Result<Uuid> uuid_too_large = Uuid::FromSpan(data_too_large);
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_large.status());
}

TEST(Uuid, UuidToString_FromSpan_byte) {
  std::array<std::byte, 16> data = {
      std::byte{0x01},
      std::byte{0x02},
      std::byte{0x03},
      std::byte{0x04},
      std::byte{0x05},
      std::byte{0x06},
      std::byte{0x07},
      std::byte{0x08},
      std::byte{0x11},
      std::byte{0x12},
      std::byte{0x13},
      std::byte{0x14},
      std::byte{0x15},
      std::byte{0x16},
      std::byte{0x17},
      std::byte{0x18},
  };
  Result<Uuid> uuid = Uuid::FromSpan(data);
  EXPECT_EQ(OkStatus(), uuid.status());
  EXPECT_EQ("18171615-1413-1211-0807-060504030201", uuid.value().ToString());
}

TEST(Uuid, UuidToString_FromSpan_byte_bad) {
  std::array<std::byte, 15> data_too_small = {};
  Result<Uuid> uuid_too_small = Uuid::FromSpan(data_too_small);
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_small.status());

  std::array<std::byte, 17> data_too_large = {};
  Result<Uuid> uuid_too_large = Uuid::FromSpan(data_too_large);
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_large.status());
}

TEST(Uuid, UuidToString_FromString) {
  constexpr Result<Uuid> uuid =
      Uuid::FromString("18171615-1413-1211-0807-060504030201");
  EXPECT_EQ(OkStatus(), uuid.status());
  EXPECT_EQ("18171615-1413-1211-0807-060504030201", uuid.value().ToString());
}

TEST(Uuid, UuidToString_FromString_bad) {
  constexpr Result<Uuid> uuid_too_small =
      Uuid::FromString("18171615-1413-1211-0807-06050403020");
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_small.status());

  constexpr Result<Uuid> uuid_too_large =
      Uuid::FromString("18171615-1413-1211-0807-0605040302011");
  EXPECT_EQ(Status::FailedPrecondition(), uuid_too_large.status());

  constexpr Result<Uuid> uuid_invalid =
      Uuid::FromString("18171615-1413-1211-0807-0605040302XX");
  EXPECT_EQ(Status::InvalidArgument(), uuid_invalid.status());
}

TEST(Uuid, UuidToString_nil) {
  constexpr Uuid uuid{};
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", uuid.ToString());
}

}  // namespace
}  // namespace pw::uuid
