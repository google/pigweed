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

#include <cpp-string/utf_codecs.h>
#include <unicode/utf8.h>

namespace bt_lib_cpp_string {

bool IsStringUTF8(std::string_view str) {
  const char* src = str.data();
  size_t src_len = str.size();
  size_t char_index = 0;

  while (char_index < src_len) {
    int32_t code_point;
    U8_NEXT(src, char_index, src_len, code_point);
    if (!IsValidCharacter(code_point))
      return false;
  }
  return true;
}

// ReadUnicodeCharacter --------------------------------------------------------

bool ReadUnicodeCharacter(const char* src,
                          size_t src_len,
                          size_t* char_index,
                          uint32_t* code_point_out) {
  // U8_NEXT expects to be able to use -1 to signal an error, so we must
  // use a signed type for code_point.  But this function returns false
  // on error anyway, so code_point_out is unsigned.
  int32_t code_point;
  U8_NEXT(src, *char_index, src_len, code_point);
  *code_point_out = static_cast<uint32_t>(code_point);

  // The ICU macro above moves to the next char, we want to point to the last
  // char consumed.
  (*char_index)--;

  // Validate the decoded value.
  return IsValidCodepoint(code_point);
}

// WriteUnicodeCharacter -------------------------------------------------------

size_t WriteUnicodeCharacter(uint32_t code_point, std::string* output) {
  if (code_point <= 0x7f) {
    // Fast path the common case of one byte.
    output->push_back(static_cast<char>(code_point));
    return 1;
  }

  // BT_LIB_U8_APPEND_UNSAFE can append up to 4 bytes.
  size_t char_offset = output->length();
  size_t original_char_offset = char_offset;
  output->resize(char_offset + U8_MAX_LENGTH);

  U8_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);

  // BT_LIB_U8_APPEND_UNSAFE will advance our pointer past the inserted
  // character, so it will represent the new length of the string.
  output->resize(char_offset);
  return char_offset - original_char_offset;
}

}  // namespace bt_lib_cpp_string
