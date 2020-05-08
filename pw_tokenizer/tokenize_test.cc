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

#include "pw_tokenizer/tokenize.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "gtest/gtest.h"
#include "pw_tokenizer/pw_tokenizer_65599_fixed_length_hash.h"
#include "pw_tokenizer_private/tokenize_test.h"
#include "pw_varint/varint.h"

namespace pw::tokenizer {
namespace {

// The hash to use for this test. This makes sure the strings are shorter than
// the configured max length to ensure this test works with any reasonable
// configuration.
template <size_t kSize>
constexpr uint32_t TestHash(const char (&string)[kSize]) {
  constexpr unsigned kTestHashLength = 48;
  static_assert(kTestHashLength <= PW_TOKENIZER_CFG_HASH_LENGTH);
  static_assert(kSize <= kTestHashLength + 1);
  return PwTokenizer65599FixedLengthHash(std::string_view(string, kSize - 1),
                                         kTestHashLength);
}

// Constructs an array with the hashed string followed by the provided bytes.
template <uint8_t... kData, size_t kSize>
constexpr auto ExpectedData(const char (&format)[kSize]) {
  const uint32_t value = TestHash(format);
  return std::array<uint8_t, sizeof(uint32_t) + sizeof...(kData)>{
      static_cast<uint8_t>(value & 0xff),
      static_cast<uint8_t>(value >> 8 & 0xff),
      static_cast<uint8_t>(value >> 16 & 0xff),
      static_cast<uint8_t>(value >> 24 & 0xff),
      kData...};
}

TEST(TokenizeStringLiteral, EmptyString_IsZero) {
  constexpr pw_TokenizerStringToken token = PW_TOKENIZE_STRING("");
  EXPECT_EQ(0u, token);
}

TEST(TokenizeStringLiteral, String_MatchesHash) {
  constexpr uint32_t token = PW_TOKENIZE_STRING("[:-)");
  EXPECT_EQ(TestHash("[:-)"), token);
}

constexpr uint32_t kGlobalToken = PW_TOKENIZE_STRING(">:-[]");

TEST(TokenizeStringLiteral, GlobalVariable_MatchesHash) {
  EXPECT_EQ(TestHash(">:-[]"), kGlobalToken);
}

class TokenizeToBuffer : public ::testing::Test {
 public:
  TokenizeToBuffer() : buffer_{} {}

 protected:
  uint8_t buffer_[64];
};

TEST_F(TokenizeToBuffer, Integer64) {
  size_t message_size = 14;
  PW_TOKENIZE_TO_BUFFER(
      buffer_,
      &message_size,
      "%" PRIu64,
      static_cast<uint64_t>(0x55555555'55555555ull));  // 0xAAAAAAAA'AAAAAAAA

  // Pattern becomes 10101010'11010101'10101010 ...
  constexpr std::array<uint8_t, 14> expected =
      ExpectedData<0xAA, 0xD5, 0xAA, 0xD5, 0xAA, 0xD5, 0xAA, 0xD5, 0xAA, 0x01>(
          "%" PRIu64);
  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, Integer64Overflow) {
  size_t message_size;

  for (size_t size = 4; size < 20; ++size) {
    message_size = size;

    PW_TOKENIZE_TO_BUFFER(
        buffer_,
        &message_size,
        "%" PRIx64,
        static_cast<uint64_t>(std::numeric_limits<int64_t>::min()));

    if (size < 14) {
      constexpr std::array<uint8_t, 4> empty = ExpectedData("%" PRIx64);
      ASSERT_EQ(sizeof(uint32_t), message_size);
      EXPECT_EQ(std::memcmp(empty.data(), &buffer_, empty.size()), 0);

      // Make sure nothing was written past the end of the buffer.
      EXPECT_TRUE(std::all_of(&buffer_[size], std::end(buffer_), [](uint8_t v) {
        return v == '\0';
      }));
    } else {
      constexpr std::array<uint8_t, 14> expected =
          ExpectedData<0xff,
                       0xff,
                       0xff,
                       0xff,
                       0xff,
                       0xff,
                       0xff,
                       0xff,
                       0xff,
                       0x01>("%" PRIx64);
      ASSERT_EQ(expected.size(), message_size);
      EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
    }
  }
}

TEST_F(TokenizeToBuffer, IntegerNegative) {
  size_t message_size = 9;
  PW_TOKENIZE_TO_BUFFER(
      buffer_, &message_size, "%" PRId32, std::numeric_limits<int32_t>::min());

  // 0x8000'0000 -zig-zag-> 0xff'ff'ff'ff'0f
  constexpr std::array<uint8_t, 9> expected =
      ExpectedData<0xff, 0xff, 0xff, 0xff, 0x0f>("%" PRId32);
  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, IntegerMin) {
  size_t message_size = 9;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "%d", -1);

  constexpr std::array<uint8_t, 5> expected = ExpectedData<0x01>("%d");
  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, IntegerDoesntFit) {
  size_t message_size = 8;
  PW_TOKENIZE_TO_BUFFER(
      buffer_, &message_size, "%" PRId32, std::numeric_limits<int32_t>::min());

  constexpr std::array<uint8_t, 4> expected = ExpectedData<>("%" PRId32);
  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, String) {
  size_t message_size = sizeof(buffer_);

  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");
  constexpr std::array<uint8_t, 10> expected =
      ExpectedData<5, '5', '4', '3', '2', '!'>("The answer is: %s");

  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, String_BufferTooSmall_TruncatesAndSetsTopStatusBit) {
  size_t message_size = 8;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");

  constexpr std::array<uint8_t, 8> truncated_1 =
      ExpectedData<0x83, '5', '4', '3'>("The answer is: %s");

  ASSERT_EQ(truncated_1.size(), message_size);
  EXPECT_EQ(std::memcmp(truncated_1.data(), buffer_, truncated_1.size()), 0);
}

TEST_F(TokenizeToBuffer, String_TwoBytesLeft_TruncatesToOneCharacter) {
  size_t message_size = 6;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");

  constexpr std::array<uint8_t, 6> truncated_2 =
      ExpectedData<0x81, '5'>("The answer is: %s");

  ASSERT_EQ(truncated_2.size(), message_size);
  EXPECT_EQ(std::memcmp(truncated_2.data(), buffer_, truncated_2.size()), 0);
}

TEST_F(TokenizeToBuffer, String_OneByteLeft_OnlyWritesTruncatedStatusByte) {
  size_t message_size = 5;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");

  std::array<uint8_t, 5> result = ExpectedData<0x80>("The answer is: %s");
  ASSERT_EQ(result.size(), message_size);
  EXPECT_EQ(std::memcmp(result.data(), buffer_, result.size()), 0);
}

TEST_F(TokenizeToBuffer, EmptyString_OneByteLeft_EncodesCorrectly) {
  size_t message_size = 5;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "");

  std::array<uint8_t, 5> result = ExpectedData<0>("The answer is: %s");
  ASSERT_EQ(result.size(), message_size);
  EXPECT_EQ(std::memcmp(result.data(), buffer_, result.size()), 0);
}

TEST_F(TokenizeToBuffer, String_ZeroBytesLeft_WritesNothing) {
  size_t message_size = 4;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");

  constexpr std::array<uint8_t, 4> empty = ExpectedData<>("The answer is: %s");
  ASSERT_EQ(empty.size(), message_size);
  EXPECT_EQ(std::memcmp(empty.data(), buffer_, empty.size()), 0);
}

TEST_F(TokenizeToBuffer, NullptrString_EncodesNull) {
  char* string = nullptr;
  size_t message_size = 9;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", string);

  std::array<uint8_t, 9> result =
      ExpectedData<4, 'N', 'U', 'L', 'L'>("The answer is: %s");
  ASSERT_EQ(result.size(), message_size);
  EXPECT_EQ(std::memcmp(result.data(), buffer_, result.size()), 0);
}

TEST_F(TokenizeToBuffer, NullptrString_BufferTooSmall_EncodesTruncatedNull) {
  char* string = nullptr;
  size_t message_size = 6;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", string);

  std::array<uint8_t, 6> result = ExpectedData<0x81, 'N'>("The answer is: %s");
  ASSERT_EQ(result.size(), message_size);
  EXPECT_EQ(std::memcmp(result.data(), buffer_, result.size()), 0);
}

TEST_F(TokenizeToBuffer, Domain_String) {
  size_t message_size = sizeof(buffer_);

  PW_TOKENIZE_TO_BUFFER_DOMAIN(
      "TEST_DOMAIN", buffer_, &message_size, "The answer was: %s", "5432!");
  constexpr std::array<uint8_t, 10> expected =
      ExpectedData<5, '5', '4', '3', '2', '!'>("The answer was: %s");

  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, TruncateArgs) {
  // Args that can't fit are dropped completely
  size_t message_size = 6;
  PW_TOKENIZE_TO_BUFFER(buffer_,
                        &message_size,
                        "%u %d",
                        static_cast<uint8_t>(0b0010'1010u),
                        0xffffff);

  constexpr std::array<uint8_t, 5> expected =
      ExpectedData<0b0101'0100u>("%u %d");
  ASSERT_EQ(expected.size(), message_size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, NoRoomForToken) {
  // Nothing is written if there isn't room for the token.
  std::memset(buffer_, '$', sizeof(buffer_));
  auto is_untouched = [](uint8_t v) { return v == '$'; };

  size_t message_size = 3;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer: \"%s\"", "5432!");
  EXPECT_EQ(0u, message_size);
  EXPECT_TRUE(std::all_of(buffer_, std::end(buffer_), is_untouched));

  message_size = 2;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "Jello, world!");
  EXPECT_EQ(0u, message_size);
  EXPECT_TRUE(std::all_of(buffer_, std::end(buffer_), is_untouched));

  message_size = 1;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "Jello!");
  EXPECT_EQ(0u, message_size);
  EXPECT_TRUE(std::all_of(buffer_, std::end(buffer_), is_untouched));

  message_size = 0;
  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "Jello?");
  EXPECT_EQ(0u, message_size);
  EXPECT_TRUE(std::all_of(buffer_, std::end(buffer_), is_untouched));
}

TEST_F(TokenizeToBuffer, C_StringShortFloat) {
  size_t size = sizeof(buffer_);
  pw_TokenizeToBufferTest_StringShortFloat(buffer_, &size);
  constexpr std::array<uint8_t, 11> expected =  // clang-format off
      ExpectedData<1, '1',                 // string '1'
                   3,                      // -2 (zig-zag encoded)
                   0x00, 0x00, 0x40, 0x40  // 3.0 in floating point
                   >(TEST_FORMAT_STRING_SHORT_FLOAT);
  ASSERT_EQ(expected.size(), size);  // clang-format on
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, C_SequentialZigZag) {
  size_t size = sizeof(buffer_);
  pw_TokenizeToBufferTest_SequentialZigZag(buffer_, &size);
  constexpr std::array<uint8_t, 18> expected =
      ExpectedData<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13>(
          TEST_FORMAT_SEQUENTIAL_ZIG_ZAG);

  ASSERT_EQ(expected.size(), size);
  EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
}

TEST_F(TokenizeToBuffer, C_Overflow) {
  std::memset(buffer_, '$', sizeof(buffer_));

  {
    size_t size = 7;
    pw_TokenizeToBufferTest_Requires8(buffer_, &size);
    constexpr std::array<uint8_t, 7> expected =
        ExpectedData<2, 'h', 'i'>(TEST_FORMAT_REQUIRES_8);
    ASSERT_EQ(expected.size(), size);
    EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
    EXPECT_EQ(buffer_[7], '$');
  }

  {
    size_t size = 8;
    pw_TokenizeToBufferTest_Requires8(buffer_, &size);
    constexpr std::array<uint8_t, 8> expected =
        ExpectedData<2, 'h', 'i', 13>(TEST_FORMAT_REQUIRES_8);
    ASSERT_EQ(expected.size(), size);
    EXPECT_EQ(std::memcmp(expected.data(), buffer_, expected.size()), 0);
    EXPECT_EQ(buffer_[8], '$');
  }
}

// Test fixture for callback and global handler. Both of these need a global
// message buffer. To keep the message buffers separate, template this on the
// derived class type.
template <typename Impl>
class GlobalMessage : public ::testing::Test {
 public:
  static void SetMessage(const uint8_t* message, size_t size) {
    ASSERT_LE(size, sizeof(message_));
    std::memcpy(message_, message, size);
    message_size_bytes_ = size;
  }

 protected:
  GlobalMessage() {
    std::memset(message_, 0, sizeof(message_));
    message_size_bytes_ = 0;
  }

  static uint8_t message_[256];
  static size_t message_size_bytes_;
};

template <typename Impl>
uint8_t GlobalMessage<Impl>::message_[256] = {};
template <typename Impl>
size_t GlobalMessage<Impl>::message_size_bytes_ = 0;

class TokenizeToCallback : public GlobalMessage<TokenizeToCallback> {};

TEST_F(TokenizeToCallback, Variety) {
  PW_TOKENIZE_TO_CALLBACK(
      SetMessage, "%s there are %x (%.2f) of them%c", "Now", 2u, 2.0f, '.');
  const auto expected =  // clang-format off
      ExpectedData<3, 'N', 'o', 'w',        // string "Now"
                   0x04,                    // unsigned 2 (zig-zag encoded)
                   0x00, 0x00, 0x00, 0x40,  // float 2.0
                   0x5C                     // char '.' (0x2E, zig-zag encoded)
                   >("%s there are %x (%.2f) of them%c");
  // clang-format on
  ASSERT_EQ(expected.size(), message_size_bytes_);
  EXPECT_EQ(std::memcmp(expected.data(), message_, expected.size()), 0);
}

TEST_F(TokenizeToCallback, Strings) {
  PW_TOKENIZE_TO_CALLBACK(SetMessage, "The answer is: %s", "5432!");
  constexpr std::array<uint8_t, 10> expected =
      ExpectedData<5, '5', '4', '3', '2', '!'>("The answer is: %s");
  ASSERT_EQ(expected.size(), message_size_bytes_);
  EXPECT_EQ(std::memcmp(expected.data(), message_, expected.size()), 0);
}

TEST_F(TokenizeToCallback, Domain_Strings) {
  PW_TOKENIZE_TO_CALLBACK_DOMAIN(
      "TEST_DOMAIN", SetMessage, "The answer is: %s", "5432!");
  constexpr std::array<uint8_t, 10> expected =
      ExpectedData<5, '5', '4', '3', '2', '!'>("The answer is: %s");
  ASSERT_EQ(expected.size(), message_size_bytes_);
  EXPECT_EQ(std::memcmp(expected.data(), message_, expected.size()), 0);
}

TEST_F(TokenizeToCallback, C_SequentialZigZag) {
  pw_TokenizeToCallbackTest_SequentialZigZag(SetMessage);

  constexpr std::array<uint8_t, 18> expected =
      ExpectedData<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13>(
          TEST_FORMAT_SEQUENTIAL_ZIG_ZAG);
  ASSERT_EQ(expected.size(), message_size_bytes_);
  EXPECT_EQ(std::memcmp(expected.data(), message_, expected.size()), 0);
}

// Hijack the PW_TOKENIZE_STRING_DOMAIN macro to capture the domain name.
#undef PW_TOKENIZE_STRING_DOMAIN
#define PW_TOKENIZE_STRING_DOMAIN(domain, string)                 \
  /* assigned to a variable */ PW_TOKENIZER_STRING_TOKEN(string); \
  tokenizer_domain = domain;                                      \
  string_literal = string

TEST_F(TokenizeToBuffer, Domain_Default) {
  const char* tokenizer_domain = nullptr;
  const char* string_literal = nullptr;

  size_t message_size = sizeof(buffer_);

  PW_TOKENIZE_TO_BUFFER(buffer_, &message_size, "The answer is: %s", "5432!");

  EXPECT_STREQ(tokenizer_domain, PW_TOKENIZER_DEFAULT_DOMAIN);
  EXPECT_STREQ(string_literal, "The answer is: %s");
}

TEST_F(TokenizeToBuffer, Domain_Specified) {
  const char* tokenizer_domain = nullptr;
  const char* string_literal = nullptr;

  size_t message_size = sizeof(buffer_);

  PW_TOKENIZE_TO_BUFFER_DOMAIN(
      "._.", buffer_, &message_size, "The answer is: %s", "5432!");

  EXPECT_STREQ(tokenizer_domain, "._.");
  EXPECT_STREQ(string_literal, "The answer is: %s");
}

TEST_F(TokenizeToCallback, Domain_Default) {
  const char* tokenizer_domain = nullptr;
  const char* string_literal = nullptr;

  PW_TOKENIZE_TO_CALLBACK(SetMessage, "The answer is: %s", "5432!");

  EXPECT_STREQ(tokenizer_domain, PW_TOKENIZER_DEFAULT_DOMAIN);
  EXPECT_STREQ(string_literal, "The answer is: %s");
}

TEST_F(TokenizeToCallback, Domain_Specified) {
  const char* tokenizer_domain = nullptr;
  const char* string_literal = nullptr;

  PW_TOKENIZE_TO_CALLBACK_DOMAIN(
      "ThisIsTheDomain", SetMessage, "The answer is: %s", "5432!");

  EXPECT_STREQ(tokenizer_domain, "ThisIsTheDomain");
  EXPECT_STREQ(string_literal, "The answer is: %s");
}

}  // namespace
}  // namespace pw::tokenizer
