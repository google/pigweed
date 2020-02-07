// Copyright 2020 The Pigweed Authors
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

#include "pw_kvs/checksum.h"

#include "gtest/gtest.h"
#include "pw_kvs/crc16_checksum.h"

namespace pw::kvs {
namespace {

using std::byte;

constexpr std::string_view kString =
    "In the beginning the Universe was created. This has made a lot of "
    "people very angry and been widely regarded as a bad move.";
constexpr uint16_t kStringCrc = 0xC184;

TEST(Checksum, UpdateAndVerify) {
  ChecksumCrc16 crc16_algo;
  ChecksumAlgorithm& algo = crc16_algo;

  algo.Update(kString.data(), kString.size());
  EXPECT_EQ(Status::OK, algo.Verify(as_bytes(span(&kStringCrc, 1))));
}

TEST(Checksum, Verify_Failure) {
  ChecksumCrc16 algo;
  EXPECT_EQ(Status::DATA_LOSS, algo.Verify(as_bytes(span(kString.data(), 2))));
}

TEST(Checksum, Verify_InvalidSize) {
  ChecksumCrc16 algo;
  EXPECT_EQ(Status::INVALID_ARGUMENT, algo.Verify({}));
  EXPECT_EQ(Status::INVALID_ARGUMENT,
            algo.Verify(as_bytes(span(kString.substr(0, 1)))));
}

TEST(Checksum, Verify_LargerState_ComparesToTruncatedData) {
  byte crc[3] = {byte{0x84}, byte{0xC1}, byte{0x33}};
  ChecksumCrc16 algo;
  ASSERT_GT(sizeof(crc), algo.size_bytes());

  algo.Update(as_bytes(span(kString)));

  EXPECT_EQ(Status::OK, algo.Verify(crc));
}

TEST(Checksum, Reset) {
  ChecksumCrc16 crc_algo;
  crc_algo.Update(as_bytes(span(kString)));
  crc_algo.Reset();

  span state = crc_algo.Finish();
  EXPECT_EQ(state[0], byte{0xFF});
  EXPECT_EQ(state[1], byte{0xFF});
}

}  // namespace
}  // namespace pw::kvs
