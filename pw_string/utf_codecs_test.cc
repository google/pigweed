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

#include "pw_string/utf_codecs.h"

#include <array>
#include <string>
#include <string_view>

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(UtfCodecs, IsValidCodepoint) {
  EXPECT_TRUE(utf::IsValidCodepoint(0u));
  EXPECT_TRUE(utf::IsValidCodepoint(0xD800u - 1u));
  EXPECT_FALSE(utf::IsValidCodepoint(0xD800u));
  EXPECT_FALSE(utf::IsValidCodepoint(0xE000u - 1u));
  EXPECT_TRUE(utf::IsValidCodepoint(0xE000u));
  EXPECT_TRUE(utf::IsValidCodepoint(0x10FFFFu));
  EXPECT_FALSE(utf::IsValidCodepoint(0x10FFFFu + 1));
  EXPECT_FALSE(utf::IsValidCodepoint(0xFFFFFFFFu));
}

TEST(UtfCodecs, IsValidCharacter) {
  EXPECT_TRUE(utf::IsValidCharacter(0u));
  EXPECT_TRUE(utf::IsValidCharacter(0xD800u - 1u));
  EXPECT_FALSE(utf::IsValidCharacter(0xD800u));
  EXPECT_FALSE(utf::IsValidCharacter(0xE000u - 1u));
  EXPECT_TRUE(utf::IsValidCharacter(0xE000u));
  EXPECT_TRUE(utf::IsValidCharacter(0xFDD0u - 1u));
  EXPECT_FALSE(utf::IsValidCharacter(0xFDD0u));
  EXPECT_FALSE(utf::IsValidCharacter(0xFDEFu));
  EXPECT_TRUE(utf::IsValidCharacter(0xFDEFu + 1u));
  EXPECT_TRUE(utf::IsValidCharacter(0x10FFFFu - 2u));
  EXPECT_FALSE(utf::IsValidCharacter(0x10FFFFu + 1u));
  EXPECT_FALSE(utf::IsValidCharacter(0xFFFEu));
  EXPECT_FALSE(utf::IsValidCharacter(0x1FFFEu));
}

TEST(UtfCodecs, IsStringUTF8) {
  EXPECT_TRUE(pw::utf8::IsStringValid("Just some ascii!"));
  EXPECT_TRUE(pw::utf8::IsStringValid("Test💖"));
  EXPECT_TRUE(pw::utf8::IsStringValid(""));
  std::array<char, 4> invalid = {
      char(0xFF), char(0xFF), char(0xFF), char(0xFF)};
  EXPECT_FALSE(pw::utf8::IsStringValid({invalid.data(), invalid.size()}));
}

TEST(UtfCodecs, ReadCharacter) {
  {
    const char str[] = "$";
    const size_t char_byte_size = sizeof(str) - 1;

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->size(), char_byte_size);
    EXPECT_EQ(rslt->code_point(), 0x0024u);
  }

  {
    const char str[] = "£";
    const size_t char_byte_size = sizeof(str) - 1;

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->size(), char_byte_size);
    EXPECT_EQ(rslt->code_point(), 0x00A3u);

    // Read too small.
    rslt = pw::utf8::ReadCodePoint({str, char_byte_size - 1});
    EXPECT_FALSE(rslt.ok());

    // Continuation bits are incorrect.
    std::string bad_continuation{str};
    uint8_t last_byte = static_cast<uint8_t>(bad_continuation.back());
    last_byte &= 0x7F;
    bad_continuation.back() = static_cast<char>(last_byte);
    rslt = pw::utf8::ReadCodePoint(bad_continuation);
    EXPECT_FALSE(rslt.ok());
  }

  {
    const char str[] = "€";
    const size_t char_byte_size = sizeof(str) - 1;

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->size(), char_byte_size);
    EXPECT_EQ(rslt->code_point(), 0x20ACu);

    // Read too small.
    rslt = pw::utf8::ReadCodePoint({str, char_byte_size - 1});
    EXPECT_FALSE(rslt.ok());

    // Continuation bits are incorrect.
    std::string bad_continuation{str};
    uint8_t last_byte = static_cast<uint8_t>(bad_continuation.back());
    last_byte &= 0x7F;
    bad_continuation.back() = static_cast<char>(last_byte);
    rslt = pw::utf8::ReadCodePoint(bad_continuation);
    EXPECT_FALSE(rslt.ok());
  }

  {
    const char str[] = "𐍈";
    const size_t char_byte_size = sizeof(str) - 1;

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->size(), char_byte_size);
    EXPECT_EQ(rslt->code_point(), 0x10348u);

    // Read too small.
    rslt = pw::utf8::ReadCodePoint({str, char_byte_size - 1});
    EXPECT_FALSE(rslt.ok());

    // Continuation bits are incorrect.
    std::string bad_continuation{str};
    uint8_t last_byte = static_cast<uint8_t>(bad_continuation.back());
    last_byte &= 0x7F;
    bad_continuation.back() = static_cast<char>(last_byte);
    rslt = pw::utf8::ReadCodePoint(bad_continuation);
    EXPECT_FALSE(rslt.ok());
  }

  {
    const char str[] = {
        char(0xFF), char(0xFF), char(0xFF), char(0xFF), char(0)};

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_FALSE(rslt.ok());
    EXPECT_TRUE(rslt.status().IsInvalidArgument());
  }

  {
    const char str[] = "";

    auto rslt = pw::utf8::ReadCodePoint(str);
    EXPECT_FALSE(rslt.ok());
    EXPECT_TRUE(rslt.status().IsInvalidArgument());
  }

  {
    // Encode a code point that ends up being an invalid utf-8 encoding.
    const uint32_t invalid_utf8_code_point = 0xD800u + 1u;
    auto encoded = pw::utf8::EncodeCodePoint(invalid_utf8_code_point);
    EXPECT_TRUE(encoded.ok());

    // Reading it back should fail validation.
    auto rslt = pw::utf8::ReadCodePoint(encoded->as_view());
    EXPECT_FALSE(rslt.ok());
    EXPECT_TRUE(rslt.status().IsOutOfRange());
  }
}

TEST(UtfCodecs, FunctionsAreConstexpr) {
  {
    constexpr char str[] = "$";
    constexpr size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x0024u;

    constexpr pw::Result<pw::utf::CodePointAndSize> rslt =
        pw::utf8::ReadCodePoint(str);
    static_assert(rslt.ok());

    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->size(), char_byte_size);
    EXPECT_EQ(rslt->code_point(), 0x0024u);

    constexpr bool valid_str = pw::utf8::IsStringValid(str);
    static_assert(valid_str);

    EXPECT_TRUE(valid_str);

    constexpr auto encoded = pw::utf8::EncodeCodePoint(code_point);
    static_assert(encoded.ok());
  }
}

TEST(UtfCodecs, WriteCodePoint) {
  {
    const char str[] = "$";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x0024u;

    std::array<char, 2> buffer{};
    pw::StringBuilder out(buffer);
    auto rslt = pw::utf8::WriteCodePoint(code_point, out);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(out.size(), char_byte_size);
    EXPECT_EQ(out, std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "£";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x00A3u;

    std::array<char, 3> buffer{};
    pw::StringBuilder out(buffer);
    auto rslt = pw::utf8::WriteCodePoint(code_point, out);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(out.size(), char_byte_size);
    EXPECT_EQ(out, std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "€";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x20ACu;

    std::array<char, 4> buffer{};
    pw::StringBuilder out(buffer);
    auto rslt = pw::utf8::WriteCodePoint(code_point, out);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(out.size(), char_byte_size);
    EXPECT_EQ(out, std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "𐍈";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x10348u;

    std::array<char, 5> buffer{};
    pw::StringBuilder out(buffer);
    auto rslt = pw::utf8::WriteCodePoint(code_point, out);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(out.size(), char_byte_size);
    EXPECT_EQ(out, std::string_view(str, char_byte_size));
  }

  {
    const uint32_t code_point = 0xFFFFFFFFu;

    std::array<char, 4> buffer{};
    pw::StringBuilder out(buffer);
    EXPECT_FALSE(pw::utf8::WriteCodePoint(code_point, out).ok());
    // Nothing should be written.
    EXPECT_EQ(out.view(), std::string_view{});
  }

  {
    const char str[] = "𐍈";
    const uint32_t code_point = 0x10348u;

    std::array<char, 3> buffer{};
    pw::StringBuilder out(buffer);
    // Buffer was too small so it should have a failure status.
    EXPECT_FALSE(pw::utf8::WriteCodePoint(code_point, out).ok());
    // We expect the first two code units to be written.
    EXPECT_EQ(out.view(), std::string_view(str, 2));
  }
}

TEST(UtfCodecs, EncodeCodePoint) {
  {
    const char str[] = "$";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x0024u;

    auto rslt = pw::utf8::EncodeCodePoint(code_point);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->as_view(), std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "£";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x00A3u;

    auto rslt = pw::utf8::EncodeCodePoint(code_point);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->as_view(), std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "€";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x20ACu;

    auto rslt = pw::utf8::EncodeCodePoint(code_point);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->as_view(), std::string_view(str, char_byte_size));
  }

  {
    const char str[] = "𐍈";
    const size_t char_byte_size = sizeof(str) - 1;
    const uint32_t code_point = 0x10348u;

    auto rslt = pw::utf8::EncodeCodePoint(code_point);
    EXPECT_TRUE(rslt.ok());
    EXPECT_EQ(rslt->as_view(), std::string_view(str, char_byte_size));
  }

  {
    const uint32_t code_point = 0xFFFFFFFFu;

    EXPECT_FALSE(pw::utf8::EncodeCodePoint(code_point).ok());
  }
}

}  // namespace
}  // namespace pw
