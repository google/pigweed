// Copyright 2021 The Pigweed Authors
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

#include "pw_log/proto_utils.h"

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_protobuf/bytes_utils.h"
#include "pw_protobuf/decoder.h"

namespace pw::log {

void VerifyLogEntry(pw::protobuf::Decoder& entry_decoder,
                    pw::log_tokenized::Metadata expected_metadata,
                    ConstByteSpan expected_tokenized_data,
                    const int64_t expected_timestamp) {
  ConstByteSpan tokenized_data;
  EXPECT_TRUE(entry_decoder.Next().ok());  // message [tokenized]
  EXPECT_EQ(1U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadBytes(&tokenized_data).ok());
  EXPECT_TRUE(std::memcmp(tokenized_data.begin(),
                          expected_tokenized_data.begin(),
                          expected_tokenized_data.size()) == 0);

  uint32_t line_level;
  EXPECT_TRUE(entry_decoder.Next().ok());  // line_level
  EXPECT_EQ(2U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadUint32(&line_level).ok());
  EXPECT_EQ(expected_metadata.level(), line_level & PW_LOG_LEVEL_BITMASK);
  EXPECT_EQ(expected_metadata.line_number(),
            (line_level & ~PW_LOG_LEVEL_BITMASK) >> PW_LOG_LEVEL_BITS);

  if (expected_metadata.flags() != 0) {
    uint32_t flags;
    EXPECT_TRUE(entry_decoder.Next().ok());  // flags
    EXPECT_EQ(3U, entry_decoder.FieldNumber());
    EXPECT_TRUE(entry_decoder.ReadUint32(&flags).ok());
    EXPECT_EQ(expected_metadata.flags(), flags);
  }

  int64_t timestamp;
  EXPECT_TRUE(entry_decoder.Next().ok());  // timestamp
  EXPECT_EQ(4U, entry_decoder.FieldNumber());
  EXPECT_TRUE(entry_decoder.ReadInt64(&timestamp).ok());
  EXPECT_EQ(expected_timestamp, timestamp);

  if (expected_metadata.module() != 0) {
    EXPECT_TRUE(entry_decoder.Next().ok());  // module name
    EXPECT_EQ(7U, entry_decoder.FieldNumber());
    const Result<uint32_t> module =
        protobuf::DecodeBytesToUint32(entry_decoder);
    ASSERT_TRUE(module.ok());
    EXPECT_EQ(expected_metadata.module(), module.value());
  }
}

TEST(UtilsTest, EncodeTokenizedLog) {
  constexpr std::byte kTokenizedData[1] = {(std::byte)0x01};
  constexpr int64_t kExpectedTimestamp = 1;
  std::byte encode_buffer[32];

  pw::log_tokenized::Metadata metadata =
      pw::log_tokenized::Metadata::Set<1, 2, 3, 4>();

  Result<ConstByteSpan> result = EncodeTokenizedLog(
      metadata, kTokenizedData, kExpectedTimestamp, encode_buffer);
  EXPECT_TRUE(result.ok());

  pw::protobuf::Decoder log_decoder(result.value());
  VerifyLogEntry(log_decoder, metadata, kTokenizedData, kExpectedTimestamp);

  result = EncodeTokenizedLog(metadata,
                              reinterpret_cast<const uint8_t*>(kTokenizedData),
                              sizeof(kTokenizedData),
                              kExpectedTimestamp,
                              encode_buffer);
  EXPECT_TRUE(result.ok());

  log_decoder.Reset(result.value());
  VerifyLogEntry(log_decoder, metadata, kTokenizedData, kExpectedTimestamp);
}

TEST(UtilsTest, EncodeTokenizedLog_EmptyFlags) {
  constexpr std::byte kTokenizedData[1] = {(std::byte)0x01};
  constexpr int64_t kExpectedTimestamp = 1;
  std::byte encode_buffer[32];

  // Create an empty flags set.
  pw::log_tokenized::Metadata metadata =
      pw::log_tokenized::Metadata::Set<1, 2, 0, 4>();

  Result<ConstByteSpan> result = EncodeTokenizedLog(
      metadata, kTokenizedData, kExpectedTimestamp, encode_buffer);
  EXPECT_TRUE(result.ok());

  pw::protobuf::Decoder log_decoder(result.value());
  VerifyLogEntry(log_decoder, metadata, kTokenizedData, kExpectedTimestamp);
}

TEST(UtilsTest, EncodeTokenizedLog_InsufficientSpace) {
  constexpr std::byte kTokenizedData[1] = {(std::byte)0x01};
  constexpr int64_t kExpectedTimestamp = 1;
  std::byte encode_buffer[1];

  pw::log_tokenized::Metadata metadata =
      pw::log_tokenized::Metadata::Set<1, 2, 3, 4>();

  Result<ConstByteSpan> result = EncodeTokenizedLog(
      metadata, kTokenizedData, kExpectedTimestamp, encode_buffer);
  EXPECT_TRUE(result.status().IsResourceExhausted());
}

}  // namespace pw::log
