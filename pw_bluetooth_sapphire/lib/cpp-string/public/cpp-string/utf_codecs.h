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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_UTF_CODECS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_UTF_CODECS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

namespace bt_lib_cpp_string {

inline bool IsValidCodepoint(uint32_t code_point) {
  // Excludes the surrogate code points ([0xD800, 0xDFFF]) and
  // codepoints larger than 0x10FFFF (the highest codepoint allowed).
  // Non-characters and unassigned codepoints are allowed.
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point <= 0x10FFFFu);
}

inline bool IsValidCharacter(uint32_t code_point) {
  // Excludes non-characters (U+FDD0..U+FDEF, and all codepoints ending in
  // 0xFFFE or 0xFFFF) from the set of valid code points.
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point < 0xFDD0u) ||
         (code_point > 0xFDEFu && code_point <= 0x10FFFFu &&
          (code_point & 0xFFFEu) != 0xFFFEu);
}

bool IsStringUTF8(std::string_view str);

// ReadUnicodeCharacter --------------------------------------------------------

// Reads a UTF-8 stream, placing the next code point into the given output
// |*code_point|. |src| represents the entire string to read, and |*char_index|
// is the character offset within the string to start reading at. |*char_index|
// will be updated to index the last character read, such that incrementing it
// (as in a for loop) will take the reader to the next character.
//
// Returns true on success. On false, |*code_point| will be invalid.
bool ReadUnicodeCharacter(const char* src,
                          size_t src_len,
                          size_t* char_index,
                          uint32_t* code_point_out);

// WriteUnicodeCharacter -------------------------------------------------------

// Appends a UTF-8 character to the given 8-bit string.  Returns the number of
// bytes written.
size_t WriteUnicodeCharacter(uint32_t code_point, std::string* output);

}  // namespace bt_lib_cpp_string

#endif  // SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_STRING_UTF_CODECS_H_
